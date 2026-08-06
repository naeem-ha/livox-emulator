#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace emu {

// Parsed .lvx2 device-info block (63 bytes on disk).
struct Lvx2Device {
  char sn[17] = {0};
  uint32_t lidar_id = 0;
  uint8_t device_type = 0;  // 9 = Mid-360
  bool extrinsic_enable = false;
  float roll = 0, pitch = 0, yaw = 0;  // deg
  float x = 0, y = 0, z = 0;           // meters
};

// One package inside a frame: 27-byte header + point payload. Payload bytes
// are wire-format points (identical layout to the UDP data plane).
struct Lvx2Package {
  uint32_t lidar_id = 0;
  uint8_t timestamp_type = 0;
  uint64_t timestamp_ns = 0;
  uint16_t udp_cnt = 0;
  uint8_t data_type = 0;  // 1 = Cartesian high (14B/pt), 2 = Cartesian low (8B/pt)
  uint8_t frame_cnt = 0;
  const uint8_t* payload = nullptr;
  uint32_t payload_len = 0;
  uint64_t frame_index = 0;
};

// mmap-backed reader for LVX2 files (LVX2 Specifications v1.0).
class Lvx2Reader {
 public:
  Lvx2Reader() = default;
  ~Lvx2Reader();
  Lvx2Reader(const Lvx2Reader&) = delete;
  Lvx2Reader& operator=(const Lvx2Reader&) = delete;

  bool open(const std::string& path, std::string& err);

  uint32_t frame_duration_ms() const { return frame_duration_ms_; }
  const std::vector<Lvx2Device>& devices() const { return devices_; }

  // Sequential package walk across frames; false at end of file.
  bool next_package(Lvx2Package& out);
  void rewind();

 private:
  bool enter_frame_(uint64_t frame_off);

  const uint8_t* map_ = nullptr;
  size_t size_ = 0;
  int fd_ = -1;

  uint32_t frame_duration_ms_ = 0;
  std::vector<Lvx2Device> devices_;

  uint64_t first_frame_off_ = 0;
  uint64_t next_frame_off_ = 0;  // start of the frame after the current one
  uint64_t frame_end_ = 0;       // end of current frame's package region
  uint64_t pkg_off_ = 0;         // cursor within current frame
  uint64_t frame_index_ = 0;
  bool in_frame_ = false;
};

}  // namespace emu
