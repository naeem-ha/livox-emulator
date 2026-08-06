#!/usr/bin/env bash
# Boot the stage-2 QEMU aarch64 guest ('virt' machine — raspi4b has no NIC
# emulation and raspi3b tops out below 1 Mbit/s; virt+virtio-net is the correct
# stand-in for the Pi 4B). Attaches tap0 (bridge br-livox) and a 9p share of
# the repo so the guest can run the cross-built binary and samples directly.
#
# Usage: run-qemu.sh <debian-12-nocloud-arm64.qcow2>
# Guest one-time setup (root, no password on nocloud images):
#   ip addr add 192.168.77.12/24 dev enp0s1 && ip link set enp0s1 up
#   mkdir -p /mnt/repo && mount -t 9p -o trans=virtio,version=9p2000.L hostshare /mnt/repo
#   /mnt/repo/build-aarch64/livox_emulator --ip 192.168.77.12
set -euo pipefail
IMG="${1:?usage: run-qemu.sh <debian-arm64.qcow2>}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"

BIOS=/usr/share/AAVMF/AAVMF_CODE.fd
[ -f "$BIOS" ] || BIOS=/usr/share/qemu-efi-aarch64/QEMU_EFI.fd

exec qemu-system-aarch64 \
  -machine virt -cpu cortex-a72 -smp 4 -m 2048 \
  -bios "$BIOS" \
  -drive if=virtio,format=qcow2,file="$IMG" \
  -device virtio-net-pci,netdev=n0,mac=52:54:00:77:00:12 \
  -netdev tap,id=n0,ifname=tap0,script=no,downscript=no \
  -virtfs "local,path=$REPO,mount_tag=hostshare,security_model=none,id=hostshare" \
  -nographic
