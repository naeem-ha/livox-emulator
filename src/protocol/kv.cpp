#include "protocol/kv.hpp"

#include <cstring>

namespace emu {
namespace {

uint16_t rd16(const uint8_t* p) {
  uint16_t v;
  std::memcpy(&v, p, 2);
  return v;
}

void wr16(std::vector<uint8_t>& out, uint16_t v) {
  out.push_back(static_cast<uint8_t>(v & 0xFF));
  out.push_back(static_cast<uint8_t>(v >> 8));
}

}  // namespace

std::optional<std::vector<KV>> parse_kv_list(const uint8_t* p, size_t len) {
  if (len < 4) return std::nullopt;
  const uint16_t n = rd16(p);
  size_t off = 4;
  std::vector<KV> out;
  out.reserve(n);
  for (uint16_t i = 0; i < n; ++i) {
    if (off + 4 > len) return std::nullopt;
    KV kv;
    kv.key = rd16(p + off);
    const uint16_t vlen = rd16(p + off + 2);
    off += 4;
    if (off + vlen > len) return std::nullopt;
    kv.value.assign(p + off, p + off + vlen);
    off += vlen;
    out.push_back(std::move(kv));
  }
  return out;
}

std::optional<std::vector<uint16_t>> parse_key_query(const uint8_t* p, size_t len) {
  if (len < 4) return std::nullopt;
  const uint16_t n = rd16(p);
  std::vector<uint16_t> keys;
  keys.reserve(n);
  if (len == 4 + static_cast<size_t>(n) * 2) {  // SDK's bare-key layout
    for (uint16_t i = 0; i < n; ++i) keys.push_back(rd16(p + 4 + i * 2));
    return keys;
  }
  if (auto kvs = parse_kv_list(p, len)) {  // wiki layout: KV entries, len=0
    for (const auto& kv : *kvs) keys.push_back(kv.key);
    return keys;
  }
  return std::nullopt;
}

std::vector<uint8_t> build_write_ack() {
  return {0x00, 0x00, 0x00};  // ret_code=0, error_key=0
}

std::vector<uint8_t> build_read_ack(const std::vector<KV>& kvs) {
  std::vector<uint8_t> out;
  out.push_back(0x00);  // ret_code
  wr16(out, static_cast<uint16_t>(kvs.size()));
  for (const auto& kv : kvs) {
    wr16(out, kv.key);
    wr16(out, static_cast<uint16_t>(kv.value.size()));
    out.insert(out.end(), kv.value.begin(), kv.value.end());
  }
  return out;
}

std::vector<uint8_t> build_kv_push(const std::vector<KV>& kvs) {
  std::vector<uint8_t> out;
  wr16(out, static_cast<uint16_t>(kvs.size()));
  wr16(out, 0);  // rsvd
  for (const auto& kv : kvs) {
    wr16(out, kv.key);
    wr16(out, static_cast<uint16_t>(kv.value.size()));
    out.insert(out.end(), kv.value.begin(), kv.value.end());
  }
  return out;
}

}  // namespace emu
