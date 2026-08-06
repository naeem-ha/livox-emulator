#!/usr/bin/env bash
# Build the ext4 payload disk the QEMU guest mounts as /dev/vdb: the static
# aarch64 emulator binary plus (if downloaded) the official lvx2 sample.
# Avoids needing 9p/ssh/network transfer into the guest.
set -euo pipefail
cd "$(dirname "$0")/.."

OUT="${1:-/root/qemu/payload.img}"
[ -f build-aarch64/livox_emulator ] || { echo "run scripts/cross-build.sh first" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
DIR=$(mktemp -d)
cp build-aarch64/livox_emulator "$DIR/"
[ -f samples/Indoor_sampledata.lvx2 ] && cp samples/Indoor_sampledata.lvx2 "$DIR/"

rm -f "$OUT"
truncate -s 512M "$OUT"
mkfs.ext4 -q -d "$DIR" "$OUT"
rm -rf "$DIR"
echo "payload disk at $OUT; guest usage:"
echo "  mkdir -p /payload && mount /dev/vdb /payload"
echo "  /payload/livox_emulator --ip 192.168.77.12 [--source lvx2 --lvx2 /payload/Indoor_sampledata.lvx2]"
