#include <cstring>

#include "doctest/doctest.h"
#include "protocol/kv.hpp"

using namespace emu;

TEST_CASE("parse kv list (0x0100 write shape)") {
  // key_num=2, rsvd, {0x0006, len 8, 8 bytes}, {0x001A, len 1, 0x01}
  const uint8_t buf[] = {0x02, 0x00, 0x00, 0x00,
                         0x06, 0x00, 0x08, 0x00, 192, 168, 77, 50, 0x25, 0xDC, 0xEC, 0xDB,
                         0x1A, 0x00, 0x01, 0x00, 0x01};
  auto kvs = parse_kv_list(buf, sizeof(buf));
  REQUIRE(kvs.has_value());
  REQUIRE(kvs->size() == 2);
  CHECK((*kvs)[0].key == 0x0006);
  CHECK((*kvs)[0].value.size() == 8);
  CHECK((*kvs)[1].key == 0x001A);
  CHECK((*kvs)[1].value == std::vector<uint8_t>{0x01});
}

TEST_CASE("parse kv list rejects truncation") {
  const uint8_t buf[] = {0x02, 0x00, 0x00, 0x00, 0x06, 0x00, 0x08, 0x00, 1, 2};
  CHECK(!parse_kv_list(buf, sizeof(buf)).has_value());
}

TEST_CASE("parse key query: SDK bare-key layout") {
  // Exactly what the SDK sends for the fw_type inquire: key_num=1, rsvd, 0x8010.
  const uint8_t buf[] = {0x01, 0x00, 0x00, 0x00, 0x10, 0x80};
  auto keys = parse_key_query(buf, sizeof(buf));
  REQUIRE(keys.has_value());
  REQUIRE(keys->size() == 1);
  CHECK((*keys)[0] == 0x8010);
}

TEST_CASE("parse key query: wiki KV layout with len=0") {
  const uint8_t buf[] = {0x02, 0x00, 0x00, 0x00,
                         0x00, 0x80, 0x00, 0x00, 0x02, 0x80, 0x00, 0x00};
  auto keys = parse_key_query(buf, sizeof(buf));
  REQUIRE(keys.has_value());
  CHECK(*keys == std::vector<uint16_t>{0x8000, 0x8002});
}

TEST_CASE("write ack bytes") {
  CHECK(build_write_ack() == std::vector<uint8_t>{0x00, 0x00, 0x00});
}

TEST_CASE("read ack bytes for fw_type") {
  // {ret=0, param_num=1, key=0x8010, len=1, value=1}
  auto out = build_read_ack({{0x8010, {0x01}}});
  CHECK(out == std::vector<uint8_t>{0x00, 0x01, 0x00, 0x10, 0x80, 0x01, 0x00, 0x01});
}

TEST_CASE("kv push framing") {
  auto out = build_kv_push({{0x0000, {0x01}}, {0x0001, {0x00}}});
  const uint8_t expect[] = {0x02, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x01, 0x00, 0x01,
                            0x01, 0x00, 0x01, 0x00, 0x00};
  CHECK(out == std::vector<uint8_t>(expect, expect + sizeof(expect)));
}
