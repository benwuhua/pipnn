# Environment Guide

User-editable. Claude reads this file before managing services. Update when ports change or new services are added.

No server processes — environment activation only.

## Activation

```bash
# optional helper env for python scripts
source .venv/bin/activate
```

## Notes
- This project is CLI/batch oriented.
- Remote lifecycle is handled by `generic-x86-remote` scripts in `scripts/bench/` and `$CODEX_HOME/skills/generic-x86-remote/scripts/`.
