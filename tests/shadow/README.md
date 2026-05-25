# Historical Shadow Tools

This directory contains the old shadow-mode comparison runner and archived
last-run logs from the core integration phase.

Shadow mode has been removed from Alfred. The default comparison mode of
`compare_shadow_output.py` now exits with an explanatory error instead of
starting a fake legacy/core comparison.

For the current product checks, use:

```bash
make test
make test-backend-diagnostics
```

For manual inspection only, the runner can still execute a historical scenario
against the official core stream:

```bash
python3 tests/shadow/compare_shadow_output.py <scenario> --event-engine core
```

Do not use this directory as a merge or pre-commit gate.
