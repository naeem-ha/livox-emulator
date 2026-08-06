#include "protocol/frame_codec.hpp"

#include <cstring>

#include "protocol/crc.hpp"

namespace emu {

std::vector<uint8_t> pack_ctrl(const CtrlFrame& f) {
  std::vector<uint8_t> out(sizeof(CtrlHeader) + f.payload.size());
  CtrlHeader h{};
  h.sof = kSof;
  h.version = kProtoVersion;
  h.length = static_cast<uint16_t>(out.size());
  h.seq_num = f.seq_num;
  h.cmd_id = f.cmd_id;
  h.cmd_type = f.cmd_type;
  h.sender_type = f.sender_type;
  h.crc32_d = f.payload.empty() ? 0 : crc32_iso_hdlc(f.payload.data(), f.payload.size());
  std::memcpy(out.data(), &h, sizeof(h));
  const uint16_t c16 = crc16_ccitt_false(out.data(), 18);
  std::memcpy(out.data() + 18, &c16, sizeof(c16));
  if (!f.payload.empty()) {
    std::memcpy(out.data() + sizeof(CtrlHeader), f.payload.data(), f.payload.size());
  }
  return out;
}

std::optional<CtrlFrame> parse_ctrl(const uint8_t* buf, size_t len) {
  if (len < sizeof(CtrlHeader)) return std::nullopt;
  CtrlHeader h{};
  std::memcpy(&h, buf, sizeof(h));
  if (h.sof != kSof || h.version != kProtoVersion) return std::nullopt;
  if (h.length < sizeof(CtrlHeader) || h.length > len) return std::nullopt;
  if (crc16_ccitt_false(buf, 18) != h.crc16_h) return std::nullopt;
  const size_t data_len = h.length - sizeof(CtrlHeader);
  const uint32_t expect = data_len == 0 ? 0 : crc32_iso_hdlc(buf + sizeof(CtrlHeader), data_len);
  if (expect != h.crc32_d) return std::nullopt;

  CtrlFrame f;
  f.seq_num = h.seq_num;
  f.cmd_id = h.cmd_id;
  f.cmd_type = h.cmd_type;
  f.sender_type = h.sender_type;
  f.payload.assign(buf + sizeof(CtrlHeader), buf + sizeof(CtrlHeader) + data_len);
  return f;
}

size_t build_data_packet(uint8_t* out, uint8_t data_type, uint16_t dot_num,
                         uint16_t udp_cnt, uint8_t frame_cnt, uint64_t timestamp_ns,
                         const uint8_t* payload, size_t payload_len,
                         uint16_t time_interval) {
  DataHeader h{};
  h.version = 0;
  h.length = static_cast<uint16_t>(sizeof(DataHeader) + payload_len);
  h.time_interval = time_interval;
  h.dot_num = dot_num;
  h.udp_cnt = udp_cnt;
  h.frame_cnt = frame_cnt;
  h.data_type = data_type;
  h.time_type = 0;
  h.timestamp_ns = timestamp_ns;
  std::memcpy(out, &h, sizeof(h));
  std::memcpy(out + sizeof(h), payload, payload_len);
  // crc32 covers timestamp (offset 28) through end of data.
  const uint32_t c32 = crc32_iso_hdlc(out + 28, 8 + payload_len);
  std::memcpy(out + 24, &c32, sizeof(c32));
  return sizeof(h) + payload_len;
}

}  // namespace emu
