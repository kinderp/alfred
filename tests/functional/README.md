# Historical Functional Tests

This directory contains the old functional suite from the legacy/shadow phase.

The scripts are kept only as migration history. They are not part of the
current verification workflow because `tests/lib/test_lib.sh` starts Alfred
with `ALFRED_EVENT_ENGINE=shadow`, and shadow mode has been removed from the
runtime.

Long-term direction: delete this directory completely once its historical value
has been migrated into the Italian documentation or into supported tests. Do
not add new tests here.

Use the supported suites instead:

```bash
make test
make test-backend-diagnostics
```

When a historical scenario still describes useful product behavior, port it to
`tests/core/` or `tests/backend/` instead of re-enabling shadow mode.
