#pragma once

#include <optional>
#include <string>

namespace emu {

struct Config {
  std::string lidar_ip = "192.168.77.12";
  std::string netmask = "255.255.255.0";
  std::string gateway = "192.168.77.1";
  std::string sn = "MID360EMU0000001";  // 16 chars max, zero-padded on the wire
  std::string source = "synthetic";     // "synthetic" | "lvx2"
  std::string lvx2_path;
  unsigned batch = 1;      // point packets per sendmmsg burst (QEMU/TCG mitigation)
  bool state_push = true;  // 1 Hz 0x0102 state-info push
  int log_level = 1;       // 0=debug 1=info 2=warn 3=error
};

// Returns std::nullopt and fills |err| on bad arguments. err == "help" for -h.
std::optional<Config> parse_args(int argc, char** argv, std::string& err);

void print_usage(const char* prog);

}  // namespace emu
