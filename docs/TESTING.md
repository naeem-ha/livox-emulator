# Testing runbooks

Three rings, cheapest first. All of this runs inside one WSL2 distro (or any Linux box);
the shared subnet is `192.168.77.0/24` — host/judge at `.50`, emulator at `.12`.

## Ring 0 — unit tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build --output-on-failure
# optional: walk a real recording end to end
LVX2_SAMPLE=samples/Indoor_sampledata.lvx2 ./build/tests/livox_emu_tests -tc='official sample*'
```

## Ring 1 — host_sim in a netns (no ROS)

`tools/host_sim/host_sim.py` impersonates the SDK host with an independent stdlib-Python
implementation (crc_hqx/zlib.crc32) and asserts everything the SDK would silently drop on:
source IPs/ports, CRC-valid acks with echoed seq, exact length fields, gapless `udp_cnt`,
rates within ±2% (2083.33 pkt/s points, 200 Hz IMU).

```bash
sudo bash scripts/netns-up.sh
sudo ip netns exec lidar ./build/livox_emulator --ip 192.168.77.12 &   # or --source lvx2 ...
python3 tools/host_sim/host_sim.py                                     # exit 0 = pass
sudo bash scripts/netns-down.sh
```

## Ring 2 — the real judge (stock livox_ros_driver2, ROS2 Humble)

Requires **native Docker Engine inside the WSL distro** (`apt install docker.io`). Docker
Desktop will not work: its `--network=host` is the docker-desktop VM's namespace, which
cannot see our veth/bridge. `scripts/run-judge.sh` refuses it.

```bash
# emulator still running in the netns as above
bash scripts/run-judge.sh                     # builds image on first run, then launches driver
# in another terminal:
docker exec -it $(docker ps -q -f ancestor=livox-judge) bash -lc \
  'source /opt/ros/humble/setup.bash && source /ws_livox/install/setup.bash && \
   ros2 topic hz /livox/lidar & ros2 topic hz /livox/imu & sleep 15; kill %1 %2'
```

**Pass criterion (the project's definition of done):** `/livox/lidar` ≈ 10 Hz with
non-empty CustomMsg (`point_num > 0`) and `/livox/imu` ≈ 200 Hz, sustained, for both
`--source synthetic` and `--source lvx2`.

## Ring 3 — QEMU aarch64 guest (stage 2, Pi 4B stand-in)

The `virt` machine type is deliberate: QEMU's `raspi4b` has **no Ethernet emulation** and
`raspi3b` (usb-net) tops out below 1 Mbit/s — useless for a 23 Mbit/s stream. `virt` +
virtio-net exercises the same aarch64 userland the Pi runs. Note this is TCG (no KVM for
aarch64-on-x86), so CPU headroom is the thing to watch; `--batch 4` batches sends via
sendmmsg if cadence slips (watch the emulator's resync counter).

```bash
sudo bash scripts/netns-down.sh         # frees 192.168.77.50
sudo bash scripts/bridge-up.sh          # br-livox + tap0 (+ tun check, rmem_max)
sudo bash scripts/make-guest-disk.sh    # ext4 payload disk: static binary + sample
sudo bash scripts/run-qemu.sh /root/qemu/debian-12-nocloud-arm64.qcow2
# inside the guest console (root, no password):
#   bash /dev/stdin < guest-setup commands printed by make-guest-disk.sh
# then from the WSL side run host_sim / run-judge.sh exactly as in rings 1-2.
```

`scripts/qemu-guest.exp` automates the console dance (login, static IP, mount payload
disk, launch emulator) if `expect` is installed.

## Raspberry Pi 4B notes (ring 4, when hardware arrives)

- `scripts/cross-build.sh` produces a fully static aarch64 binary — no library setup on
  the Pi. Copy it (and optionally a `.lvx2`) over, then run with `--ip <pi-ip>`.
- Give the Pi's `eth0` a static IP and put the same IP in the host's
  `MID360_config.json` `lidar_configs[0].ip`. Keep it different from the host's IP.
- Multi-homed SLAM hosts need `ip route add 255.255.255.255/32 dev <iface-to-pi>` or the
  discovery broadcast leaves via the wrong NIC (classic "driver never finds the lidar").
- Raise `net.core.rmem_max` on the SLAM host (the SDK asks for a 200 MB receive buffer).
- If IMU jitter matters, pin the streamer threads (`taskset`) and consider `--batch 2`.
