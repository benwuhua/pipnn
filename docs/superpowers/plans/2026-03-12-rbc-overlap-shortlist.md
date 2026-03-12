# RBC-Overlap Shortlist Implementation Plan

> **For agentic workers:** REQUIRED: Use superpowers:subagent-driven-development (if subagents available) or superpowers:executing-plans to implement this plan. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the regressing exact leaf full-scan candidate generation path with an `RBC`-overlap shortlist path that builds smaller per-point candidate sets from leaf memberships before exact distance selection and `HashPrune`.

**Architecture:** Keep the current `RBC` tree construction, overlap semantics, `HashPrune`, and search behavior intact. Expose stable point-to-leaf memberships from `RBC`, add a focused shortlist helper that unions the permitted leaf ids for each point under a deterministic candidate cap, and route `pipnn_builder.cpp` through this shortlist path instead of treating each leaf as an exact full-scan work unit. Validate locally with deterministic membership/shortlist tests, then remotely on the existing high-dimensional `simplewiki-openai` smoke workload.

**Tech Stack:** C++20, CMake, existing `ctest` suite, current `Eigen` dependency already used by `RBC` batching, generic-x86-remote wrappers, existing `simplewiki-openai` high-dim benchmark scripts.

---

## File Map

- Modify: `src/core/rbc.h`
  - Extend the `RBC` build result so builder code can consume stable point-to-leaf membership information directly.
- Modify: `src/core/rbc.cpp`
  - Populate the membership output while keeping the current tree construction, `fanout`, and overlap rules unchanged.
- Create: `src/core/leaf_candidates.h`
  - Declare a small internal API for membership-based shortlist construction, deterministic cap enforcement, and exact top-k over a shortlist.
- Create: `src/core/leaf_candidates.cpp`
  - Implement shortlist generation from leaf memberships and exact candidate scoring for one point.
- Modify: `src/core/pipnn_builder.cpp`
  - Replace the current exact leaf full-scan candidate generation path with the shortlist path and keep the interface to `HashPrune` unchanged.
- Modify: `src/core/leaf_knn.cpp`
  - Demote the old leaf full-scan path to legacy/small-leaf helper status only if needed by tests or fallback logic; it must no longer be the primary builder path.
- Modify: `tests/test_rbc.cpp`
  - Add deterministic checks for exposed memberships and overlap bounds.
- Modify: `tests/test_pipnn_integration.cpp`
  - Add builder-level tests for shortlist candidate provenance, cap enforcement, and graph sanity.
- Create or Modify: `tests/test_leaf_candidates.cpp`
  - Add focused unit tests for membership-to-shortlist translation and exact shortlist top-k behavior.
- Modify: `results/high_dim_validation/README.md`
  - Record the post-change high-dimensional smoke numbers and the algorithm decision.
- Modify if needed: `scripts/bench/run_simplewiki_openai_20k_100.sh`
  - Keep the existing remote smoke wrapper reusable for `5k/50` and optional `20k/100` follow-up without editing the script body between runs.

## Chunk 1: Expose and Lock RBC Membership Semantics

### Task 1: Add failing tests for membership exposure and bounded overlap

**Files:**
- Modify: `tests/test_rbc.cpp`
- Modify: `src/core/rbc.h`
- Modify: `src/core/rbc.cpp`

- [ ] **Step 1: Write the failing membership tests**

Extend `tests/test_rbc.cpp` so it asserts directly on the future membership output from `BuildRbcLeaves(...)`.

Cover these behaviors:

- every input point appears in at least one leaf membership list
- maximum membership count is bounded by the current overlap policy for the deterministic fixture
- the exposed point-to-leaf view is deterministic across repeated runs on the same fixture
- a point never appears in a leaf it was not assigned to by the current `RBC` result

Use a small fixture that already exercises overlap, for example a line or clustered 2D dataset with `fanout=2`.

- [ ] **Step 2: Run the targeted test to verify red**

Run:

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: build or test failure because the `RBC` result does not yet expose the needed membership information.

- [ ] **Step 3: Extend the RBC result surface and populate memberships**

Update `src/core/rbc.h/.cpp` so the result contains a stable, builder-facing membership representation. Keep it local and explicit.

A reasonable target shape is:

```cpp
namespace pipnn {
struct RbcResult {
  std::vector<std::vector<int>> leaves;
  std::vector<std::vector<int>> point_memberships;
  // existing diagnostics...
};
}  // namespace pipnn
```

Requirements:

- keep current leaf content unchanged
- populate `point_memberships[u]` with leaf indices that contain `u`
- keep ordering deterministic so later shortlist truncation remains reproducible
- do not redesign the existing tree or overlap assignment

- [ ] **Step 4: Re-run the focused RBC test to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^rbc$'
```

Expected: `rbc` passes with the new membership output.

- [ ] **Step 5: Commit**

```bash
git add tests/test_rbc.cpp src/core/rbc.h src/core/rbc.cpp
git commit -m "test: lock rbc membership semantics"
```

## Chunk 2: Add the Membership-Based Shortlist Helper

### Task 2: Implement deterministic shortlist generation and exact shortlist scoring

**Files:**
- Create: `src/core/leaf_candidates.h`
- Create: `src/core/leaf_candidates.cpp`
- Create or Modify: `tests/test_leaf_candidates.cpp`

- [ ] **Step 1: Add failing shortlist helper tests**

Create focused tests for a future helper API. Cover these behaviors:

- shortlist is the deduplicated union of the point's allowed leaves
- self is always excluded
- deterministic cap enforcement produces the same truncated shortlist on repeated runs
- exact distance + top-k over the shortlist returns the expected neighbors on a fixed fixture
- no candidate outside the permitted leaf union is ever emitted

Target a minimal helper surface like:

```cpp
namespace pipnn {
struct ShortlistConfig {
  int candidate_cap = 0;
};

std::vector<int> BuildShortlistForPoint(
    int point_id,
    const std::vector<std::vector<int>>& leaves,
    const std::vector<std::vector<int>>& point_memberships,
    const ShortlistConfig& cfg = {});

std::vector<Edge> ScoreShortlistExact(
    const Matrix& points,
    int point_id,
    const std::vector<int>& shortlist,
    int k,
    bool bidirected);
}  // namespace pipnn
```

- [ ] **Step 2: Run the targeted test to verify red**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(leaf_candidates|leaf_knn)$'
```

Expected: build or test failure because the helper API does not exist yet.

- [ ] **Step 3: Add the helper declarations and green implementation**

Implement `src/core/leaf_candidates.h/.cpp` with these constraints:

- candidate order must be deterministic
- candidate union may use a temporary `seen` array or similar fast dedupe structure
- self exclusion is mandatory before cap enforcement
- initial `candidate_cap` must remain a small internal policy surface, with the first builder use targeting `max_degree * 8`
- exact scoring must compute true distances on the shortlist and keep exact top-k over that shortlist
- bidirected edge emission must match the current builder expectations

- [ ] **Step 4: Re-run the focused shortlist tests to verify green**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(leaf_candidates|leaf_knn)$'
```

Expected: shortlist helper tests pass.

- [ ] **Step 5: Commit**

```bash
git add src/core/leaf_candidates.h src/core/leaf_candidates.cpp tests/test_leaf_candidates.cpp
git commit -m "feat: add rbc overlap shortlist helper"
```

## Chunk 3: Route the Builder Through the Shortlist Path

### Task 3: Replace exact leaf full-scan candidate generation in `pipnn_builder.cpp`

**Files:**
- Modify: `src/core/pipnn_builder.cpp`
- Modify: `src/core/leaf_knn.cpp`
- Modify: `tests/test_pipnn_integration.cpp`
- Test: `tests/test_leaf_candidates.cpp`

- [ ] **Step 1: Add failing integration assertions if current coverage is insufficient**

Review `tests/test_pipnn_integration.cpp` and add focused builder-level assertions only where coverage is missing.

Useful checks:

- candidate generation only draws from the point's permitted leaf union
- candidate count per point respects the deterministic cap before `HashPrune`
- graph construction still produces sane edge counts and prune counters on a deterministic fixture

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure -R '^(pipnn_integration|leaf_candidates)$'
```

Expected: red only if a new assertion was added.

- [ ] **Step 2: Replace the primary candidate-generation path**

In `src/core/pipnn_builder.cpp`:

- keep current `RBC` invocation
- stop treating leaves as full-scan work units in the primary path
- iterate points, build a shortlist from `point_memberships`, score exact top-k on that shortlist, and append to the existing per-point candidate buffers consumed by `HashPrune`
- keep prune accounting and downstream graph creation unchanged

Do not change `HashPrune`, query search, or the `RBC` tree structure in this chunk.

- [ ] **Step 3: Demote the legacy leaf full-scan path**

Refactor `src/core/leaf_knn.cpp` so:

- it is no longer the default builder path
- any retained code clearly serves legacy/small-leaf helper or reference-test use only
- dead per-leaf batching code from prior experiments is not reintroduced

- [ ] **Step 4: Re-run the local regression suite**

Run:

```bash
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Expected: full local suite passes with the builder now routed through shortlist generation.

- [ ] **Step 5: Commit**

```bash
git add src/core/pipnn_builder.cpp src/core/leaf_knn.cpp tests/test_pipnn_integration.cpp
git commit -m "refactor: route builder through shortlist candidates"
```

## Chunk 4: Validate on the High-Dimensional Workload and Decide Promotion

### Task 4: Run remote smoke and record the algorithm decision

**Files:**
- Modify: `results/high_dim_validation/README.md`
- Modify if needed: `scripts/bench/run_simplewiki_openai_20k_100.sh`

- [ ] **Step 1: Run the authoritative high-dimensional smoke**

Use the existing remote wrapper and keep the primary smoke workload fixed at:

- dataset: `/data/work/datasets/crisp/simplewiki-openai`
- slice: `5k/50`
- metric: `l2`

Run the current PiPNN path and collect:

- `build_sec`
- `partition_sec`
- shortlist-generation or equivalent candidate-build time if exposed separately
- `prune_sec`
- `recall_at_10`
- `edges`

If the result is clearly promising, follow up with `20k/100` using the same script surface.

- [ ] **Step 2: Compare against the existing checkpoints**

Use these reference numbers in the write-up:

- post-`RBC` batching checkpoint:
  - `build_sec = 30.4769`
  - `partition_sec = 12.5956`
  - `leaf_knn_sec = 8.13263`
  - `recall_at_10 = 0.978`
- failed exact multi-leaf batching result:
  - `build_sec = 66.6072`
  - `leaf_knn_sec = 42.5382`
  - `recall_at_10 = 0.978`

The shortlist iteration is informative if it materially beats the failed exact path and preserves acceptable recall. It is promotion-ready only if it approaches or beats the post-`RBC` checkpoint.

- [ ] **Step 3: Update the high-dimensional validation report**

Update `results/high_dim_validation/README.md` with:

- the new numbers
- a concise explanation of whether candidate reduction is a better direction than exact leaf batching
- the next architecture decision

Be explicit if the result is only informative and not promotion-ready.

- [ ] **Step 4: Commit**

```bash
git add results/high_dim_validation/README.md scripts/bench/run_simplewiki_openai_20k_100.sh
git commit -m "chore: refresh shortlist high-dim evidence"
```

## Final Verification

- [ ] Run the full local regression suite:

```bash
ctest --test-dir build --output-on-failure
```

- [ ] Run the authoritative remote high-dimensional smoke and collect the metrics artifacts.

- [ ] Confirm the final state before requesting review:

```bash
git status --short
```

## Notes

- Keep `candidate_cap` deterministic in the first implementation. Do not add a CLI knob unless the smoke result proves the policy needs tuning.
- Do not reintroduce exact leaf batching variants into the primary path during this iteration.
- If the shortlist path still fails to beat the post-`RBC` checkpoint, treat that as real evidence that `RBC` membership alone is too weak a candidate generator for this workload.
