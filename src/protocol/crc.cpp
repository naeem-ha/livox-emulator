#include "protocol/crc.hpp"

#include <array>

namespace emu {
namespace {

std::array<uint16_t, 256> make_crc16_table() {
  std::array<uint16_t, 256> t{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint16_t crc = static_cast<uint16_t>(i << 8);
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                           : static_cast<uint16_t>(crc << 1);
    }
    t[i] = crc;
  }
  return t;
}

std::array<uint32_t, 256> make_crc32_table() {
  std::array<uint32_t, 256> t{};
  for (uint32_t i = 0; i < 256; ++i) {
    uint32_t crc = i;
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 1) ? (crc >> 1) ^ 0xEDB88320u : crc >> 1;
    }
    t[i] = crc;
  }
  return t;
}

const std::array<uint16_t, 256> kCrc16Table = make_crc16_table();
const std::array<uint32_t, 256> kCrc32Table = make_crc32_table();

}  // namespace

uint16_t crc16_ccitt_false(const uint8_t* data, size_t len) {
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < len; ++i) {
    crc = static_cast<uint16_t>((crc << 8) ^ kCrc16Table[((crc >> 8) ^ data[i]) & 0xFF]);
  }
  return crc;
}

uint32_t crc32_iso_hdlc(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; ++i) {
    crc = (crc >> 8) ^ kCrc32Table[(crc ^ data[i]) & 0xFF];
  }
  return crc ^ 0xFFFFFFFFu;
}

}  // namespace emu
