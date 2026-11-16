#!/usr/bin/env bash
# Download the pinned PS5 Payload SDK; verify before extracting.
set -euo pipefail
root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
sdk="$root/.deps/ps5-payload-sdk"
if [[ -x "$sdk/bin/prospero-clang" ]]; then
    echo "SDK already present: $sdk"
    exit 0
fi
mkdir -p "$root/.deps"
archive="$root/.deps/ps5-payload-sdk-v0.42.zip"
curl -fL 'https://github.com/ps5-payload-dev/sdk/releases/download/v0.42/ps5-payload-sdk.zip' -o "$archive.download"
python3 - "$archive.download" <<'PY'
import hashlib,sys
from pathlib import Path
expected='8cfbc7cd5811e719eb4f0c47eea668d3dc7b40bc8ab11c4a5031d40c23ec02da'
if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()!=expected:
    raise SystemExit('SDK SHA-256 mismatch; not extracting')
PY
mv "$archive.download" "$archive"
unzip -q "$archive" -d "$root/.deps"
test -x "$sdk/bin/prospero-clang"
echo "SDK ready: $sdk"
