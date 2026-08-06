#include "util/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace emu {

void print_usage(const char* prog) {
  std::fprintf(stderr,
               "Livox Mid-360 emulator\n"
               "usage: %s [options]\n"
               "  --ip <addr>          lidar IP to report/bind (default 192.168.77.12)\n"
               "  --netmask <addr>     reported netmask (default 255.255.255.0)\n"
               "  --gateway <addr>     reported gateway (default 192.168.77.1)\n"
               "  --sn <str>           serial number, <=16 chars (default MID360EMU0000001)\n"
               "  --source <name>      point source: synthetic | lvx2 (default synthetic)\n"
               "  --lvx2 <path>        .lvx2 file to replay (required for --source lvx2)\n"
               "  --batch <n>          point packets per send burst, 1-32 (default 1)\n"
               "  --state-push <0|1>   1 Hz 0x0102 state push (default 1)\n"
               "  --log-level <n>      0=debug 1=info 2=warn 3=error (default 1)\n"
               "  -h, --help           show this help\n",
               prog);
}

std::optional<Config> parse_args(int argc, char** argv, std::string& err) {
  Config cfg;
  auto need_value = [&](int i) -> const char* {
    if (i + 1 >= argc) return nullptr;
    return argv[i + 1];
  };
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    const char* v = nullptr;
    if (!std::strcmp(a, "-h") || !std::strcmp(a, "--help")) {
      err = "help";
      return std::nullopt;
    } else if (!std::strcmp(a, "--ip") && (v = need_value(i))) {
      cfg.lidar_ip = v; ++i;
    } else if (!std::strcmp(a, "--netmask") && (v = need_value(i))) {
      cfg.netmask = v; ++i;
    } else if (!std::strcmp(a, "--gateway") && (v = need_value(i))) {
      cfg.gateway = v; ++i;
    } else if (!std::strcmp(a, "--sn") && (v = need_value(i))) {
      cfg.sn = v; ++i;
    } else if (!std::strcmp(a, "--source") && (v = need_value(i))) {
      cfg.source = v; ++i;
    } else if (!std::strcmp(a, "--lvx2") && (v = need_value(i))) {
      cfg.lvx2_path = v; ++i;
    } else if (!std::strcmp(a, "--batch") && (v = need_value(i))) {
      cfg.batch = static_cast<unsigned>(std::strtoul(v, nullptr, 10)); ++i;
    } else if (!std::strcmp(a, "--state-push") && (v = need_value(i))) {
      cfg.state_push = std::strtoul(v, nullptr, 10) != 0; ++i;
    } else if (!std::strcmp(a, "--log-level") && (v = need_value(i))) {
      cfg.log_level = static_cast<int>(std::strtol(v, nullptr, 10)); ++i;
    } else {
      err = std::string("unknown or incomplete argument: ") + a;
      return std::nullopt;
    }
  }
  if (cfg.sn.size() > 16) {
    err = "--sn must be at most 16 characters";
    return std::nullopt;
  }
  if (cfg.source != "synthetic" && cfg.source != "lvx2") {
    err = "--source must be 'synthetic' or 'lvx2'";
    return std::nullopt;
  }
  if (cfg.source == "lvx2" && cfg.lvx2_path.empty()) {
    err = "--source lvx2 requires --lvx2 <path>";
    return std::nullopt;
  }
  if (cfg.batch < 1 || cfg.batch > 32) {
    err = "--batch must be in 1..32";
    return std::nullopt;
  }
  return cfg;
}

}  // namespace emu
