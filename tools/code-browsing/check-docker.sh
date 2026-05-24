#!/usr/bin/env bash

set -euo pipefail

if ! command -v docker >/dev/null 2>&1; then
    echo "Docker non trovato nel PATH." >&2
    echo "Installa Docker Engine o Docker Desktop prima di usare i browser del codice." >&2
    exit 1
fi

if ! docker info >/dev/null 2>&1; then
    echo "Docker non risponde oppure l'utente corrente non ha i permessi." >&2
    echo "Controlla che il daemon sia avviato e che l'utente possa eseguire docker." >&2
    exit 1
fi

if docker compose version >/dev/null 2>&1; then
    COMPOSE_CMD="docker compose"
elif command -v docker-compose >/dev/null 2>&1; then
    COMPOSE_CMD="docker-compose"
else
    COMPOSE_CMD="non disponibile"
fi

cat <<MSG
Docker disponibile.

Docker:
  $(docker --version)

Compose:
  $COMPOSE_CMD
MSG
