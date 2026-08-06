#include "doctest/doctest.h"
#include "protocol/crc.hpp"

using namespace emu;

namespace {
const uint8_t kCheck[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
}

TEST_CASE("crc16 ccitt-false known answers") {
  // Standard check value for CRC-16/CCITT-FALSE.
  CHECK(crc16_ccitt_false(kCheck, sizeof(kCheck)) == 0x29B1);
  // Empty input returns the init value.
  CHECK(crc16_ccitt_false(nullptr, 0) == 0xFFFF);
}

TEST_CASE("crc32 iso-hdlc known answers") {
  // Standard check value for CRC-32 (zlib).
  CHECK(crc32_iso_hdlc(kCheck, sizeof(kCheck)) == 0xCBF43926);
  // zlib crc32 of empty input is 0 — this is what makes the SDK's
  // "crc32_d = 0 for empty payload" convention self-consistent.
  CHECK(crc32_iso_hdlc(nullptr, 0) == 0);
}
