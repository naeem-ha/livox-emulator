// Wire-format structs and constants for the Livox Mid-360 UDP protocol.
// Layouts transcribed from Livox-SDK2 (MIT license):
//   include/livox_lidar_def.h, sdk_core/comm/{define.h,sdk_protocol.h}
// and the official Mid-360 communication protocol wiki page.
#pragma once

#include <cstddef>
#include <cstdint>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "Livox wire format is little-endian; big-endian hosts are not supported"
#endif

namespace emu {

#pragma pack(push, 1)

struct CtrlHeader {
  uint8_t sof;          // 0xAA
  uint8_t version;      // 0
  uint16_t length;      // whole frame: 24 + payload
  uint32_t seq_num;     // ACK echoes the REQ's seq
  uint16_t cmd_id;
  uint8_t cmd_type;     // 0 REQ, 1 ACK
  uint8_t sender_type;  // 0 host, 1 lidar
  uint8_t rsvd[6];
  uint16_t crc16_h;     // CRC-16/CCITT-FALSE over bytes 0..17
  uint32_t crc32_d;     // CRC-32 over payload; literal 0 when payload empty
};
static_assert(sizeof(CtrlHeader) == 24, "CtrlHeader must be 24 bytes");

struct DetectionAckData {
  uint8_t ret_code;   // 0 = success
  uint8_t dev_type;   // 9 = Mid-360
  char sn[16];        // zero-padded
  uint8_t lidar_ip[4];
  uint16_t cmd_port;  // 56100
};
static_assert(sizeof(DetectionAckData) == 24, "DetectionAckData must be 24 bytes");

struct DataHeader {
  uint8_t version;        // 0
  uint16_t length;        // whole datagram: 36 + payload (SDK trusts this field)
  uint16_t time_interval; // sampling span of this packet, 0.1 us units
  uint16_t dot_num;       // points in packet; must be != 0 (driver divides by it)
  uint16_t udp_cnt;       // +1 per packet, wraps
  uint8_t frame_cnt;      // +1 per point-cloud frame
  uint8_t data_type;      // 0 IMU, 1 Cartesian-high, 2 Cartesian-low, 3 spherical
  uint8_t time_type;      // 0 no-sync (host-clock publish path in driver)
  uint8_t rsvd[12];
  uint32_t crc32;         // over timestamp+data (bytes 28..); SDK never checks it
  uint64_t timestamp_ns;
};
static_assert(sizeof(DataHeader) == 36, "DataHeader must be 36 bytes");

struct PointCartHigh {  // data_type 1
  int32_t x;  // mm
  int32_t y;
  int32_t z;
  uint8_t reflectivity;
  uint8_t tag;
};
static_assert(sizeof(PointCartHigh) == 14, "PointCartHigh must be 14 bytes");

struct PointCartLow {  // data_type 2
  int16_t x;  // 10 mm units
  int16_t y;
  int16_t z;
  uint8_t reflectivity;
  uint8_t tag;
};
static_assert(sizeof(PointCartLow) == 8, "PointCartLow must be 8 bytes");

struct ImuSample {  // data_type 0
  float gyro_x;  // rad/s
  float gyro_y;
  float gyro_z;
  float acc_x;  // g
  float acc_y;
  float acc_z;
};
static_assert(sizeof(ImuSample) == 24, "ImuSample must be 24 bytes");

struct HostEndpointCfg {  // value layout of keys 0x0005/0x0006/0x0007
  uint8_t ip[4];
  uint16_t host_port;
  uint16_t lidar_port;
};
static_assert(sizeof(HostEndpointCfg) == 8, "HostEndpointCfg must be 8 bytes");

#pragma pack(pop)

// --- constants -------------------------------------------------------------

constexpr uint8_t kSof = 0xAA;
constexpr uint8_t kProtoVersion = 0;

constexpr uint8_t kCmdTypeReq = 0x00;
constexpr uint8_t kCmdTypeAck = 0x01;
constexpr uint8_t kSenderHost = 0x00;
constexpr uint8_t kSenderLidar = 0x01;

constexpr uint8_t kDevTypeMid360 = 9;

enum CmdId : uint16_t {
  kCmdDiscovery = 0x0000,
  kCmdParamWrite = 0x0100,   // "parameter configuration"
  kCmdParamRead = 0x0101,    // "parameter inquire"
  kCmdPushStateInfo = 0x0102,
  kCmdRebootDevice = 0x0200,
  kCmdResetDevice = 0x0201,
};

enum ParamKey : uint16_t {
  kKeyPclDataType = 0x0000,
  kKeyPatternMode = 0x0001,
  kKeyLidarIpCfg = 0x0004,        // ip[4]+mask[4]+gw[4]
  kKeyStateInfoHostCfg = 0x0005,  // HostEndpointCfg
  kKeyPointHostCfg = 0x0006,
  kKeyImuHostCfg = 0x0007,
  kKeyInstallAttitude = 0x0012,   // f32 roll,pitch,yaw + i32 x,y,z(mm)
  kKeyWorkMode = 0x001A,
  kKeyImuDataEn = 0x001C,
  kKeySn = 0x8000,
  kKeyProductInfo = 0x8001,
  kKeyVersionApp = 0x8002,
  kKeyVersionLoader = 0x8003,
  kKeyVersionHardware = 0x8004,
  kKeyMac = 0x8005,
  kKeyCurWorkState = 0x8006,
  kKeyCoreTemp = 0x8007,  // i32, 0.01 degC
  kKeyFwType = 0x8010,    // must answer non-zero or SDK treats us as bootloader
};

// Lidar-side ports (fixed by the SDK's ParamsCheck; not configurable).
constexpr uint16_t kPortDetection = 56000;
constexpr uint16_t kPortCmd = 56100;
constexpr uint16_t kPortPush = 56200;
constexpr uint16_t kPortPoint = 56300;
constexpr uint16_t kPortImu = 56400;
constexpr uint16_t kPortLog = 56500;

constexpr uint8_t kDataTypeImu = 0;
constexpr uint8_t kDataTypeCartHigh = 1;
constexpr uint8_t kDataTypeCartLow = 2;

constexpr uint16_t kPointsPerPacket = 96;
constexpr size_t kPointPayloadBytes = kPointsPerPacket * sizeof(PointCartHigh);  // 1344
constexpr size_t kPointPacketBytes = sizeof(DataHeader) + kPointPayloadBytes;    // 1380
constexpr uint16_t kTimeIntervalPer96 = 4800;  // 96 pts @ 200 kpts/s = 480 us = 4800 x 0.1us

constexpr uint64_t kPointPeriodNs = 480'000;    // 2083.33 packets/s
constexpr uint64_t kImuPeriodNs = 5'000'000;    // 200 Hz
constexpr uint64_t kFramePeriodNs = 100'000'000;  // frame_cnt increments at 10 Hz

}  // namespace emu
