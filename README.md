# Livox Mid-360 Emulator

A lightweight C++17 daemon that impersonates a [Livox Mid-360](https://www.livoxtech.com/mid-360)
LiDAR on the wire (UDP), convincingly enough that **stock, unmodified
[livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2) discovers it and publishes
point clouds** (`/livox/lidar` CustomMsg + `/livox/imu`). Downstream SLAM stacks should not be
able to tell the difference.

Runs on any Linux box; the intended deployment target is a Raspberry Pi 4B wired to the SLAM
host. Validated against the real ROS2 Humble driver in a QEMU aarch64 guest.

## What it implements

- **Control plane** (spec: Mid-360 communication protocol): discovery responder on
  `0.0.0.0:56000`, command server on `:56100` (parameter configuration `0x0100`, inquire
  `0x0101`, reboot/reset acks), 1 Hz state-info push (`0x0102`) from `:56200`.
- **Data plane**: Cartesian-high point packets (96 pts, 1380 B) from `:56300` at
  2083.3 pkt/s (= 200,000 pts/s) and 200 Hz IMU packets from `:56400`, paced by
  `clock_nanosleep(TIMER_ABSTIME)` with drift resync.
- **Pluggable point sources**: `.lvx2` replay (official Livox Viewer 2 recordings, looped,
  re-chunked to uniform 96-point packets) and a synthetic debug pattern. A live-transducer
  source (e.g. masking an Ouster through the Mid-360 wire format) can slot into the same
  `PointSource` interface later.

See [docs/PROTOCOL.md](docs/PROTOCOL.md) for the wire-format details (and the sharp edges of
the SDK's undocumented source-IP/port filtering) and [docs/TESTING.md](docs/TESTING.md) for the
validation runbooks.

## Quick start (WSL2 / any Linux)

```bash
# build + unit tests
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
ctest --test-dir build

# stage-1 harness: emulator in a netns at 192.168.77.12, host side at .50
sudo bash scripts/netns-up.sh
sudo ip netns exec lidar ./build/livox_emulator --ip 192.168.77.12   # synthetic source
# or replay a real recording:
bash scripts/get-samples.sh
sudo ip netns exec lidar ./build/livox_emulator --ip 192.168.77.12 \
    --source lvx2 --lvx2 samples/Indoor_sampledata.lvx2

# validate without ROS (handshake + packet rates/shapes):
python3 tools/host_sim/host_sim.py

# validate with the real judge (ROS2 Humble container, native Docker Engine):
bash scripts/run-judge.sh          # then: ros2 topic hz /livox/lidar
```

## Raspberry Pi 4B

```bash
bash scripts/cross-build.sh        # -> build-aarch64/livox_emulator (static aarch64 ELF)
```

Copy the binary (and optionally a `.lvx2` recording) to the Pi, give its Ethernet port the
lidar IP (default `192.168.1.12` on real hardware networks — pass `--ip`), and run it. The
SLAM host's `MID360_config.json` must list that IP in `lidar_configs[0].ip`. If the host is
multi-homed, add `ip route add 255.255.255.255/32 dev <iface>` so the SDK's discovery
broadcast leaves on the right interface.

## Repo layout

| Path | What |
|---|---|
| `src/protocol/` | CRC, packed wire structs, control-frame codec, KV lists |
| `src/control/` | discovery/command/state-push server |
| `src/data/` | paced point + IMU streamers |
| `src/source/` | `PointSource` interface, synthetic + lvx2 replay, lvx2 parser |
| `tools/host_sim/` | stdlib-Python SDK-host impersonator (primary debug judge) |
| `scripts/` | netns/bridge/QEMU/judge harness |
| `docker/` | stock livox_ros_driver2 judge image |
