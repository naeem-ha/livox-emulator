#include <cstring>

#include "doctest/doctest.h"
#include "protocol/frame_codec.hpp"

using namespace emu;

TEST_CASE("pack/parse round trip") {
  CtrlFrame f;
  f.seq_num = 42;
  f.cmd_id = kCmdParamWrite;
  f.cmd_type = kCmdTypeAck;
  f.sender_type = kSenderLidar;
  f.payload = {0x00, 0x00, 0x00};

  auto bytes = pack_ctrl(f);
  REQUIRE(bytes.size() == 27);
  CHECK(bytes[0] == 0xAA);
  CHECK(bytes[1] == 0x00);
  uint16_t length;
  std::memcpy(&length, bytes.data() + 2, 2);
  CHECK(length == 27);

  auto parsed = parse_ctrl(bytes.data(), bytes.size());
  REQUIRE(parsed.has_value());
  CHECK(parsed->seq_num == 42);
  CHECK(parsed->cmd_id == kCmdParamWrite);
  CHECK(parsed->cmd_type == kCmdTypeAck);
  CHECK(parsed->sender_type == kSenderLidar);
  CHECK(parsed->payload == f.payload);
}

TEST_CASE("empty payload has literal zero crc32_d") {
  CtrlFrame f;
  f.cmd_id = kCmdDiscovery;
  auto bytes = pack_ctrl(f);
  REQUIRE(bytes.size() == 24);
  uint32_t crc32_d;
  std::memcpy(&crc32_d, bytes.data() + 20, 4);
  CHECK(crc32_d == 0);
  CHECK(parse_ctrl(bytes.data(), bytes.size()).has_value());
}

TEST_CASE("parse rejects corruption") {
  CtrlFrame f;
  f.seq_num = 7;
  f.cmd_id = kCmdParamRead;
  f.payload = {1, 2, 3, 4};
  auto good = pack_ctrl(f);
  REQUIRE(parse_ctrl(good.data(), good.size()).has_value());

  auto bad = good;
  bad[0] = 0xAB;  // sof
  CHECK(!parse_ctrl(bad.data(), bad.size()).has_value());

  bad = good;
  bad[1] = 1;  // version
  CHECK(!parse_ctrl(bad.data(), bad.size()).has_value());

  bad = good;
  bad[5] ^= 0xFF;  // seq byte -> header crc mismatch
  CHECK(!parse_ctrl(bad.data(), bad.size()).has_value());

  bad = good;
  bad.back() ^= 0xFF;  // payload byte -> crc32 mismatch
  CHECK(!parse_ctrl(bad.data(), bad.size()).has_value());

  CHECK(!parse_ctrl(good.data(), 10).has_value());  // truncated
}

TEST_CASE("data packet layout") {
  uint8_t payload[kPointPayloadBytes] = {};
  payload[0] = 0x5A;
  uint8_t out[kPointPacketBytes];
  const size_t n = build_data_packet(out, kDataTypeCartHigh, kPointsPerPacket,
                                     /*udp_cnt=*/513, /*frame_cnt=*/7,
                                     /*timestamp_ns=*/0x0102030405060708ull, payload,
                                     sizeof(payload), kTimeIntervalPer96);
  CHECK(n == 1380);
  DataHeader h;
  std::memcpy(&h, out, sizeof(h));
  CHECK(h.version == 0);
  CHECK(h.length == 1380);
  CHECK(h.time_interval == 4800);
  CHECK(h.dot_num == 96);
  CHECK(h.udp_cnt == 513);
  CHECK(h.frame_cnt == 7);
  CHECK(h.data_type == 1);
  CHECK(h.time_type == 0);
  CHECK(h.timestamp_ns == 0x0102030405060708ull);
  CHECK(out[36] == 0x5A);
}
