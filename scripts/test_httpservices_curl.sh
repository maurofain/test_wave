#!/usr/bin/env bash
# Wrapper che invoca lo script Python di test HTTP services.

set -euo pipefail

python3 "$(dirname "$0")/test_httpservices.py" "$@"
