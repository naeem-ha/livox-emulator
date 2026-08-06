# Mid-360 wire protocol — as implemented by this emulator

Everything below was verified against the official
[Mid-360 communication protocol wiki](https://livox-wiki-en.readthedocs.io/en/latest/tutorials/new_product/mid360/livox_eth_protocol_mid360.html)
and the [Livox-SDK2](https://github.com/Livox-SDK/Livox-SDK2) source (the judge's actual
parser — where the wiki and the SDK disagree, the SDK wins). All fields little-endian,
structs packed.

## Ports

| Plane | LiDAR side (fixed by SDK's ParamsCheck) | Host side (configurable) |
|---|---|---|
| discovery | 56000 | 56000 (SDK binds host_ip:56000) |
| command | 56100 | 56101 |
| state push | 56200 | 56201 |
| point cloud | 56300 | 56301 |
| IMU | 56400 | 56401 |
| log | 56500 | 56501 |

## Control frame (24 B header + payload)

```
u8  sof = 0xAA        u8  version = 0       u16 length = 24 + payload
u32 seq_num           u16 cmd_id            u8 cmd_type (0 REQ / 1 ACK)
u8  sender_type (0 host / 1 lidar)          u8 rsvd[6]
u16 crc16_h           u32 crc32_d           u8 data[]
```

- `crc16_h`: **CRC-16/CCITT-FALSE** (poly 0x1021, init 0xFFFF, no reflect/xorout) over
  bytes 0..17.
- `crc32_d`: **CRC-32 zlib** over the payload only; written as literal `0` when the payload
  is empty.
- ⚠ The `0x4c49` / `0x564f580a` seeds floating around the internet are **Livox SDK v1**
  (Mid-40/Horizon era) — wrong for the Mid-360.
- The SDK validates control frames strictly (sof, version, length, both CRCs) and computes
  the payload CRC over `length - 24` bytes taken from the **length field**.
- ACKs must **echo the request's `seq_num`** — that's the only correlation key.

## Handshake (what livox_ros_driver2 + SDK2 actually do)

1. Host broadcasts `0x0000` (empty payload) from `host_ip:56000` to
   `255.255.255.255:56000`, once per second, forever.
2. Device replies **from source port 56000**: `DetectionData{u8 ret=0, u8 dev_type=9,
   char sn[16], u8 lidar_ip[4], u16 cmd_port=56100}`.
3. Host inquires `0x0101` for key `0x8010` (fw_type) from `host:56101`. The request uses the
   SDK's *bare-key* layout (`u16 key_num, u16 rsvd, u16 key...` — no per-key length). Reply
   `{u8 ret=0, u16 param_num=1, u16 key, u16 len=1, u8 fw_type}` with **fw_type != 0**
   (zero means bootloader and the SDK stops talking to us).
4. Host writes `0x0100` with keys `0x0005/0x0006/0x0007` (state/point/IMU host endpoints,
   each `ip[4] + u16 host_port + u16 lidar_port`). Ack `{u8 ret=0, u16 error_key=0}` — this
   is the one ack whose *values* the SDK checks.
5. The ROS driver then writes, one key per request: `0x0000 pcl_data_type=1`,
   `0x0001 pattern_mode=0`, `0x0012 install_attitude`, `0x001A work_mode=1`,
   `0x001C imu_data_en=1`. **Acking pcl_data_type and pattern_mode is mandatory** — those
   two acks are what move the driver to `kConnectStateSampling`; without them it never
   publishes, no matter how much data arrives.
6. Optional: device pushes `0x0102` (KV list of state info) from `:56200` to the configured
   state endpoint at ~1 Hz.

### Undocumented gates in DeviceManager::OnData (the usual failure modes)

- The device's **source IP is its identity**: it must equal `lidar_configs[0].ip` in
  `MID360_config.json` (or the driver treats it as "not user-configured" and never
  publishes) and must **differ** from the host IP (else dropped).
- Every packet is filtered by **source port**: acks/pushes must come from 56100/56200,
  points from 56300, IMU from 56400. Anything else is dropped **silently**.
- The discovery listener must bind `0.0.0.0` — limited-broadcast datagrams are not
  delivered to unicast-bound sockets.
- On a multi-homed host, `255.255.255.255` routes out the default interface; the harness
  adds `ip route add 255.255.255.255/32 dev <iface>` to pin it.

## Data plane (36 B header + payload)

```
u8  version = 0       u16 length = 36 + payload (SDK trusts this FIELD)
u16 time_interval (0.1 us; 4800)             u16 dot_num (96; 0 => SIGFPE in driver!)
u16 udp_cnt (+1/packet)                      u8 frame_cnt (+1/frame, 10 Hz)
u8  data_type (0 IMU / 1 Cart-high)          u8 time_type = 0 (no-sync)
u8  rsvd[12]          u32 crc32 (over timestamp+data; SDK never checks)
u64 timestamp_ns      u8 data[]
```

- Points (`data_type=1`): 96 × `{i32 x,y,z (mm), u8 reflectivity, u8 tag}` = 1344 B,
  packet = 1380 B, 2083.33 pkt/s = 200k pts/s ≈ 23 Mbit/s.
- IMU (`data_type=0`): `6 × f32 {gyro xyz (rad/s), acc xyz (g)}`, dot_num=1, length=60,
  200 Hz.
- `time_type=0` is deliberate: it selects the driver's wall-clock frame-assembly path.
  Types 1/2 (PTP/GPS) use a modulo-of-timestamp publish condition that can silently never
  fire — never copy time_type from a replay file.
- The SDK does **no** validation of data packets beyond routing on `data_type`, but it
  *does* read the `length` field to size a copy — an inflated value over-reads the heap.

## .lvx2 replay

LVX2 Specifications v1.0 container: 24 B public header (`livox_tech`, magic `0xAC0EA767`),
5 B private header (frame_duration=50 ms, device_count), 63 B device blocks, then frames
(24 B header with absolute cur/next offsets) of 27 B packages
(`version, lidar_id u32, lidar_type, ts_type, ts u64, udp_cnt u16, data_type, length u32
(payload only), frame_cnt, rsvd[4]`).

Package point payloads are **byte-identical** to the wire format, so replay is: re-chunk
points into uniform 96-point packets (packages can be short at frame boundaries), rebuild
the 36 B header (fresh timestamps, `time_type=0`, running `udp_cnt`), loop at EOF. LVX2
stores no IMU data — the IMU stream is always synthesized (stationary: acc≈(0,0,1 g),
gyro≈0, gaussian noise).
