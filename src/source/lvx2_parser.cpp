#include "source/lvx2_parser.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstring>

#include "util/log.hpp"

namespace emu {
namespace {

constexpr uint32_t kMagic = 0xAC0EA767;
constexpr size_t kPublicHeader = 24;
constexpr size_t kPrivateHeader = 5;
constexpr size_t kDeviceInfo = 63;
constexpr size_t kFrameHeader = 24;
constexpr size_t kPackageHeader = 27;

template <typename T>
T rd(const uint8_t* p) {
  T v;
  std::memcpy(&v, p, sizeof(T));
  return v;
}

}  // namespace

Lvx2Reader::~Lvx2Reader() {
  if (map_) ::munmap(const_cast<uint8_t*>(map_), size_);
  if (fd_ >= 0) ::close(fd_);
}

bool Lvx2Reader::open(const std::string& path, std::string& err) {
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    err = "cannot open " + path;
    return false;
  }
  struct stat st{};
  if (::fstat(fd_, &st) != 0 || st.st_size <= 0) {
    err = "cannot stat " + path;
    return false;
  }
  size_ = static_cast<size_t>(st.st_size);
  void* m = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
  if (m == MAP_FAILED) {
    err = "mmap failed for " + path;
    return false;
  }
  map_ = static_cast<const uint8_t*>(m);

  if (size_ < kPublicHeader + kPrivateHeader) {
    err = "file too small for lvx2 headers";
    return false;
  }
  if (std::memcmp(map_, "livox_tech", 10) != 0) {
    err = "bad lvx2 signature (expected 'livox_tech')";
    return false;
  }
  if (rd<uint32_t>(map_ + 20) != kMagic) {
    err = "bad lvx2 magic code";
    return false;
  }
  frame_duration_ms_ = rd<uint32_t>(map_ + 24);
  const uint8_t dev_count = map_[28];
  if (size_ < kPublicHeader + kPrivateHeader + dev_count * kDeviceInfo) {
    err = "file too small for device info block";
    return false;
  }
  devices_.clear();
  for (uint8_t i = 0; i < dev_count; ++i) {
    const uint8_t* d = map_ + kPublicHeader + kPrivateHeader + i * kDeviceInfo;
    Lvx2Device dev;
    std::memcpy(dev.sn, d, 16);
    dev.lidar_id = rd<uint32_t>(d + 32);
    dev.device_type = d[37];
    dev.extrinsic_enable = d[38] != 0;
    dev.roll = rd<float>(d + 39);
    dev.pitch = rd<float>(d + 43);
    dev.yaw = rd<float>(d + 47);
    dev.x = rd<float>(d + 51);
    dev.y = rd<float>(d + 55);
    dev.z = rd<float>(d + 59);
    devices_.push_back(dev);
    if (dev.device_type != 9) {
      LOG_WARN("lvx2 device %u has device_type %u (not Mid-360=9); replaying anyway",
               i, dev.device_type);
    }
  }
  first_frame_off_ = kPublicHeader + kPrivateHeader + dev_count * kDeviceInfo;
  rewind();
  return true;
}

void Lvx2Reader::rewind() {
  next_frame_off_ = first_frame_off_;
  in_frame_ = false;
}

bool Lvx2Reader::enter_frame_(uint64_t frame_off) {
  if (frame_off + kFrameHeader > size_) return false;
  const uint64_t cur = rd<uint64_t>(map_ + frame_off);
  const uint64_t next = rd<uint64_t>(map_ + frame_off + 8);
  frame_index_ = rd<uint64_t>(map_ + frame_off + 16);
  if (cur != frame_off) {
    LOG_WARN("lvx2 frame header current_offset mismatch (%llu != %llu); stopping",
             static_cast<unsigned long long>(cur),
             static_cast<unsigned long long>(frame_off));
    return false;
  }
  if (next <= frame_off + kFrameHeader) return false;  // malformed; stop
  frame_end_ = next > size_ ? size_ : next;
  pkg_off_ = frame_off + kFrameHeader;
  next_frame_off_ = next;
  in_frame_ = true;
  return true;
}

bool Lvx2Reader::next_package(Lvx2Package& out) {
  for (;;) {
    if (!in_frame_) {
      if (next_frame_off_ + kFrameHeader > size_) return false;  // EOF
      if (!enter_frame_(next_frame_off_)) return false;
    }
    if (pkg_off_ + kPackageHeader > frame_end_) {
      in_frame_ = false;  // frame exhausted; advance to next
      continue;
    }
    const uint8_t* p = map_ + pkg_off_;
    const uint32_t length = rd<uint32_t>(p + 18);
    if (pkg_off_ + kPackageHeader + length > frame_end_) {
      LOG_WARN("lvx2 package overruns frame boundary; skipping rest of frame");
      in_frame_ = false;
      continue;
    }
    out.lidar_id = rd<uint32_t>(p + 1);
    out.timestamp_type = p[6];
    out.timestamp_ns = rd<uint64_t>(p + 7);
    out.udp_cnt = rd<uint16_t>(p + 15);
    out.data_type = p[17];
    out.payload_len = length;
    out.frame_cnt = p[22];
    out.payload = p + kPackageHeader;
    out.frame_index = frame_index_;
    pkg_off_ += kPackageHeader + length;
    return true;
  }
}

}  // namespace emu
