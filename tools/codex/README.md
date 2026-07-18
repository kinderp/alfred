# Codex Workstation Profiles

This directory contains reusable Codex workstation profiles for Alfred
development.

The files are sanitized templates. They intentionally do not include
authentication state, OAuth data, project trust paths, plugin caches, tokens, or
machine-specific paths.

## Sol Ultra Profile

Install the profile with:

```sh
sh tools/codex/install-sol-ultra.sh
```

On Windows, run from PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File tools\codex\install-sol-ultra.ps1
```

If an existing `config.toml` is already present, the installer refuses to
overwrite it unless an explicit force option is passed:

```sh
sh tools/codex/install-sol-ultra.sh --force
```

```powershell
powershell -ExecutionPolicy Bypass -File tools\codex\install-sol-ultra.ps1 -Force
```

When forced, the old config is backed up under:

```text
~/.codex/backups/
```

On Windows the default location is:

```text
%USERPROFILE%\.codex\backups\
```

The Italian student-facing explanation is in:

```text
docs/it/52-configurazione-codex-sol-ultra.md
```
