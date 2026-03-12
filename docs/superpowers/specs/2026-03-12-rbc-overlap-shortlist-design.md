# RBC-Overlap Shortlist Candidate Generation Design

- Date: 2026-03-12
- Status: Draft for user review
- Scope: replace the failing exact leaf full-scan candidate generation path with an `RBC`-overlap shortlist strategy that uses leaf memberships to build smaller per-point candidate sets before exact distance selection

## 1. Goal

This iteration answers one narrow question:

Can we improve build performance by shrinking the candidate set itself, instead of continuing to optimize exact full-scan leaf work that has already failed multiple times?

The current high-dimensional checkpoint after `RBC` batching is:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- `build_sec = 30.4769`
- `partition_sec = 12.5956`
- `leaf_knn_sec = 8.13263`
- `prune_sec = 9.74774`
- `recall_at_10 = 0.978`

Two subsequent leaf-side attempts were both negative:

- exact per-leaf blocked batching:
  - `build_sec = 38.6283`
  - `leaf_knn_sec = 16.1486`
  - `recall_at_10 = 0.978`
- exact multi-leaf batching:
  - `build_sec = 66.6072`
  - `leaf_knn_sec = 42.5382`
  - `recall_at_10 = 0.978`

The current evidence is strong enough that exact leaf batching is no longer the productive direction.

## 2. Why The Direction Changes Here

The failure pattern matters:

- `RBC` batching improved the high-dimensional workload materially
- all exact leaf-batching variants regressed
- recall stayed almost unchanged during those regressions

This means the current problem is not "we have not found the right leaf kernel yet". It means the system is still doing too much exact leaf work.

The next iteration should therefore attack candidate-set size, not leaf-kernel throughput.

## 3. Non-Goals

This iteration does not:

- change `HashPrune`
- change search behavior
- change the current `RBC` tree structure or overlap semantics
- create a project-wide shared distance engine
- preserve exact full-scan leaf semantics
- introduce a leader-centric intermediate graph or a new indexing layer

This is a focused change to candidate generation only.

## 4. Proposed Approach

Recommended approach: use `RBC` overlap memberships as a shortlist generator.

For each point:

1. collect the leaves it belongs to
2. union the points from those leaves
3. remove self
4. cap the union if necessary
5. run exact distance + top-k only on that shortlist
6. feed the result into the existing `HashPrune`

The important change is that the system no longer treats each leaf as a full-scan work unit. It treats leaf memberships as a localized candidate source.

## 5. Candidate Definition

### 5.1 Membership Source

This iteration depends on stable access to point-to-leaf memberships from the existing `RBC` output.

The builder needs, for each point:

- its primary leaf
- any additional overlap leaves assigned by the current `fanout` / overlap policy

This is the existing locality structure. The iteration makes it explicit and usable by the candidate builder.

### 5.2 Shortlist Construction

For one point `u`:

1. read `u`'s leaf membership list
2. collect all point ids from those leaves into a temporary set or deduplicated vector
3. remove `u`
4. if the candidate count exceeds a configured cap, truncate deterministically
5. compute exact distances from `u` to the remaining shortlist
6. keep exact top-k on that shortlist

This iteration changes the candidate set deliberately. It is not required to match the old full-scan exact path.

### 5.3 Candidate Budget

Introduce an internal candidate cap for the shortlist path.

The first implementation should keep the policy simple:

- deterministic
- local to the builder or a small helper
- no new CLI knobs unless profiling proves they are necessary

The first implementation should also fix an explicit starting budget so planning does not invent one ad hoc. For this iteration, the budget should be treated as an internal constant with a conservative default, for example:

- `candidate_cap = max_degree * 8`

The exact constant may be revised during implementation only if tests and smoke results show it is clearly too small or too large.

The goal of the first pass is to test whether candidate reduction itself is the right structural move.

## 6. Correctness Requirements

This iteration no longer uses exact full-scan equality as its main correctness rule.

Instead, correctness means:

- shortlist candidates come only from the point's allowed leaf union
- self is excluded
- exact distance computation over the shortlist is still correct
- top-k over the shortlist is exact
- no cross-structure contamination occurs outside the permitted leaf union

If a point receives candidates that are not in its permitted leaf union, the shortlist builder is wrong.

## 7. Components

### 7.1 `src/core/rbc.h` / `src/core/rbc.cpp`

This iteration may need a small extension so the existing `RBC` build result exposes stable membership information in a form the builder can consume directly.

The goal is not to redesign `RBC`. The goal is to make the already-computed locality information explicit.

### 7.2 Optional small helper: `src/core/leaf_candidates.{h,cpp}`

Recommended if the logic starts to bloat `pipnn_builder.cpp`.

This helper would own:

- membership-to-shortlist translation
- candidate deduplication
- candidate-cap enforcement
- exact shortlist scoring for one point

This keeps responsibilities cleaner than pushing all candidate logic into `leaf_knn.cpp`.

### 7.3 `src/core/pipnn_builder.cpp`

Primary orchestration site.

Expected refactor:

- keep current `RBC` invocation
- replace the current exact leaf full-scan candidate generation path
- consume membership information to build per-point shortlists
- emit per-point candidate vectors exactly where `HashPrune` expects them

### 7.4 `src/core/leaf_knn.cpp`

Demote this file from the main path.

Expected role after the iteration:

- keep small-leaf or legacy helper behavior if still useful for reference
- do not remain the primary algorithmic path for candidate generation

### 7.5 Tests

- `tests/test_rbc.cpp`
  - extend if needed to validate membership exposure
- `tests/test_pipnn_integration.cpp`
  - builder-level candidate-generation and graph sanity checks
- optional new helper tests if a dedicated shortlist helper file is introduced

## 8. Testing Strategy

### 8.1 Unit / deterministic checks

Add tests for:

- point memberships are exposed deterministically
- shortlist contains only points from the permitted leaf union
- self is excluded
- candidate cap is enforced deterministically
- exact top-k over the shortlist behaves as expected on fixed fixtures

### 8.2 Local regression suite

Run full local `ctest` after the change.

### 8.3 Remote high-dimensional smoke

Keep using:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- metric: `l2`

If the result is promising, follow up on:

- `20k/100`

## 9. Success Criteria

This iteration is informative if all of the following hold:

- local tests pass
- shortlist generation respects the permitted leaf union
- remote high-dimensional smoke keeps recall in the current acceptable range
- `build_sec` is materially below `66.6072`
- `leaf_knn_sec` is materially below `42.5382`

This iteration is promotion-ready only if it also approaches or beats the post-`RBC` baseline:

- `build_sec <= 30.4769`
- recall does not fall to an unacceptable level relative to the current `0.978` smoke result

## 10. Risks

### Risk 1: Recall drops too much

Mitigation:

- keep the shortlist source strictly tied to `RBC` overlap memberships
- keep the first candidate-cap policy conservative
- evaluate on the same high-dimensional workload first

### Risk 2: Membership plumbing bloats `rbc.cpp` or `pipnn_builder.cpp`

Mitigation:

- expose only the minimal membership data needed
- extract shortlist logic into a focused helper if file responsibilities start to blur

### Risk 3: Candidate caps introduce noisy behavior

Mitigation:

- keep truncation deterministic
- test fixtures should assert exact shortlist content for fixed inputs

## 11. Next Decision After This Iteration

After the `RBC`-overlap shortlist run, the architecture choice should be evidence-based:

- if candidate reduction materially improves build time at acceptable recall, keep investing in candidate-generation strategy
- if it does not, move to a different structural direction, such as:
  - a broader shared distance-batching layer with a different cost model
  - or a leader-centric candidate-generation path
