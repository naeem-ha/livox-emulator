#!/usr/bin/env bash
# Stage-1 harness: emulator lives in netns "lidar" (192.168.77.12), the judge
# (host_sim or the ROS driver) runs in the root netns as 192.168.77.50.
set -euo pipefail

NS=lidar
HOST_IP=192.168.77.50
LIDAR_IP=192.168.77.12

ip netns add "$NS" 2>/dev/null || true
if ! ip link show veth-h &>/dev/null; then
  ip link add veth-h type veth peer name veth-l
  ip link set veth-l netns "$NS"
fi
ip addr replace "$HOST_IP/24" dev veth-h
ip link set veth-h up
ip netns exec "$NS" ip addr replace "$LIDAR_IP/24" dev veth-l
ip netns exec "$NS" ip link set veth-l up
ip netns exec "$NS" ip link set lo up

# The SDK broadcasts 0x0000 to 255.255.255.255; without this route the kernel
# sends it out the default interface (WSL eth0) instead of the veth.
ip route replace 255.255.255.255/32 dev veth-h

# The SDK requests a huge SO_RCVBUF; give the kernel room to grant a real one.
sysctl -qw net.core.rmem_max=67108864

echo "netns '$NS' ready: host $HOST_IP (veth-h) <-> lidar $LIDAR_IP (veth-l)"
echo "run the emulator with:"
echo "  sudo ip netns exec $NS ./build/livox_emulator --ip $LIDAR_IP"
