#pragma once

#include <cstddef>
#include <cstdint>

namespace emu {

// CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, no reflection, no xorout.
// check("123456789") == 0x29B1. Covers control-frame header bytes 0..17.
uint16_t crc16_ccitt_false(const uint8_t* data, size_t len);

// CRC-32 (ISO-HDLC / zlib): poly 0x04C11DB7 reflected, init/xorout 0xFFFFFFFF.
// check("123456789") == 0xCBF43926. Covers control payload / data timestamp+points.
uint32_t crc32_iso_hdlc(const uint8_t* data, size_t len);

}  // namespace emu
