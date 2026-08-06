#include <cstring>
#include <vector>

#include "doctest/doctest.h"
#include "protocol/frame_codec.hpp"

using namespace emu;

// Golden bytes computed independently with Python:
//   binascii.crc_hqx(hdr, 0xFFFF) for the header CRC,
//   zlib.crc32(payload) for the data CRC.
// Detection ACK: seq 0x11223344, dev_type 9, sn "TESTSN0000000001",
// lidar_ip 192.168.77.12, cmd_port 56100.
TEST_CASE("detection ack matches externally computed golden frame") {
  DetectionAckData ack{};
  ack.ret_code = 0;
  ack.dev_type = kDevTypeMid360;
  std::memcpy(ack.sn, "TESTSN0000000001", 16);
  ack.lidar_ip[0] = 192;
  ack.lidar_ip[1] = 168;
  ack.lidar_ip[2] = 77;
  ack.lidar_ip[3] = 12;
  ack.cmd_port = kPortCmd;

  CtrlFrame f;
  f.seq_num = 0x11223344;
  f.cmd_id = kCmdDiscovery;
  f.cmd_type = kCmdTypeAck;
  f.sender_type = kSenderLidar;
  f.payload.resize(sizeof(ack));
  std::memcpy(f.payload.data(), &ack, sizeof(ack));

  const std::vector<uint8_t> golden = {
      0xAA, 0x00, 0x30, 0x00, 0x44, 0x33, 0x22, 0x11, 0x00, 0x00, 0x01, 0x01,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xB6, 0x0B, 0x9C, 0x75, 0xD4, 0x60,
      0x00, 0x09, 0x54, 0x45, 0x53, 0x54, 0x53, 0x4E, 0x30, 0x30, 0x30, 0x30,
      0x30, 0x30, 0x30, 0x30, 0x30, 0x31, 0xC0, 0xA8, 0x4D, 0x0C, 0x24, 0xDB};

  CHECK(pack_ctrl(f) == golden);
}
