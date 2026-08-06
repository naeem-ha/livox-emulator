#!/usr/bin/env bash
set -euo pipefail
ip netns del lidar 2>/dev/null || true
ip link del veth-h 2>/dev/null || true
ip route del 255.255.255.255/32 2>/dev/null || true
echo "netns harness removed"
