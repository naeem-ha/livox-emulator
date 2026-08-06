#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace emu {

struct KV {
  uint16_t key = 0;
  std::vector<uint8_t> value;
};

// 0x0100 write payload: u16 key_num, u16 rsvd, then {u16 key, u16 len, bytes}.
std::optional<std::vector<KV>> parse_kv_list(const uint8_t* p, size_t len);

// 0x0101 inquire payload. The SDK sends bare keys (u16 key_num, u16 rsvd,
// then u16 keys with no length field); the wiki documents KV entries with
// len=0. Accept both.
std::optional<std::vector<uint16_t>> parse_key_query(const uint8_t* p, size_t len);

// 0x0100 ACK payload: {u8 ret_code=0, u16 error_key=0}.
std::vector<uint8_t> build_write_ack();

// 0x0101 ACK payload: {u8 ret_code=0, u16 param_num, {u16 key,u16 len,bytes}...}.
std::vector<uint8_t> build_read_ack(const std::vector<KV>& kvs);

// 0x0102 push payload: {u16 key_num, u16 rsvd, {u16 key,u16 len,bytes}...}.
std::vector<uint8_t> build_kv_push(const std::vector<KV>& kvs);

}  // namespace emu
