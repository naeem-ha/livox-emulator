#!/usr/bin/env bash
# Stage-2 harness: bridge br-livox (host side, 192.168.77.50) + tap0 for the
# QEMU aarch64 guest (which statically configures 192.168.77.12).
# NOTE: uses the same subnet as the stage-1 netns — run scripts/netns-down.sh first.
set -euo pipefail

if [ ! -e /dev/net/tun ]; then
  echo "ERROR: /dev/net/tun is missing. Update WSL (wsl --update from Windows)" >&2
  echo "or enable CONFIG_TUN via a custom kernel in .wslconfig." >&2
  exit 1
fi

ip link add br-livox type bridge 2>/dev/null || true
ip addr replace 192.168.77.50/24 dev br-livox
ip link set br-livox up

ip tuntap add dev tap0 mode tap 2>/dev/null || true
ip link set tap0 master br-livox
ip link set tap0 up

# See netns-up.sh: keep the SDK's limited broadcast on this segment.
ip route replace 255.255.255.255/32 dev br-livox
sysctl -qw net.core.rmem_max=67108864

echo "bridge br-livox up at 192.168.77.50/24 with tap0 attached"
