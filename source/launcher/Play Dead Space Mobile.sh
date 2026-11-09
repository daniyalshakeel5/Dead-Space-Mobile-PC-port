#!/usr/bin/env bash
# Release entry point; forward arguments without evaluating them as shell code.
set -euo pipefail
ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
exec bash "$ROOT/Play-Dead-Space-Touch.sh" "$@"
