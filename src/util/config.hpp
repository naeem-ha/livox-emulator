#pragma once

#include <optional>
#include <string>

namespace emu {

struct Config {
  std::string lidar_ip = "192.168.77.12";
  std::string netmask = "255.255.255.0";
  std::string gateway = "192.168.77.1";
  // Max 15 chars: the wire field is char[16], but Livox-SDK2 strcpy's it into
  // a 16-byte buffer — a 16-char SN has no NUL and crashes the stock driver
  // (fortify abort in GeneralCommandHandler::LivoxLidarInfoChange). Real
  // Mid-360 serials are 14 chars.
  std::string sn = "MID360EMU00001";
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
