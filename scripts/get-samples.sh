#!/usr/bin/env bash
# Download the official Livox Mid-360 sample recordings (linked from
# https://www.livoxtech.com/mid-360/downloads).
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p samples
BASE="https://terra-1-g.djicdn.com/65c028cd298f4669a7f0e40e50ba1131/Mid360"
for f in Indoor_sampledata.lvx2 Outdoor_sampledata.lvx2; do
  if [ ! -s "samples/$f" ]; then
    echo "downloading $f ..."
    curl -L -o "samples/$f" "$BASE/$f"
  fi
done
ls -lh samples/
