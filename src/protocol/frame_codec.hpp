#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "protocol/frames.hpp"

namespace emu {

struct CtrlFrame {
  uint32_t seq_num = 0;
  uint16_t cmd_id = 0;
  uint8_t cmd_type = kCmdTypeReq;
  uint8_t sender_type = kSenderHost;
  std::vector<uint8_t> payload;
};

// Serializes header+payload with both CRCs. crc32_d is literal 0 for an empty
// payload, matching Livox-SDK2's SdkProtocol::Pack.
std::vector<uint8_t> pack_ctrl(const CtrlFrame& f);

// Mirrors Livox-SDK2 CheckPreamble: validates sof, version, length, crc16 over
// bytes 0..17 and crc32 over (length-24) payload bytes. Returns nullopt on any
// mismatch.
std::optional<CtrlFrame> parse_ctrl(const uint8_t* buf, size_t len);

// Writes a complete data-plane packet (36B header + payload) into |out|, which
// must hold at least sizeof(DataHeader)+payload_len bytes. Returns total size.
// time_type is always 0 (no-sync) by design.
size_t build_data_packet(uint8_t* out, uint8_t data_type, uint16_t dot_num,
                         uint16_t udp_cnt, uint8_t frame_cnt, uint64_t timestamp_ns,
                         const uint8_t* payload, size_t payload_len,
                         uint16_t time_interval);

}  // namespace emu
