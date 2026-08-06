#include <csignal>
#include <cstdio>
#include <memory>

#include "control/control_plane.hpp"
#include "data/imu_streamer.hpp"
#include "data/point_streamer.hpp"
#include "source/lvx2_source.hpp"
#include "source/synthetic_source.hpp"
#include "util/config.hpp"
#include "util/log.hpp"

namespace {
std::atomic<bool> g_stop{false};
void on_signal(int) { g_stop.store(true); }
}  // namespace

int main(int argc, char** argv) {
  std::string err;
  auto cfg = emu::parse_args(argc, argv, err);
  if (!cfg) {
    if (err == "help") {
      emu::print_usage(argv[0]);
      return 0;
    }
    std::fprintf(stderr, "error: %s\n", err.c_str());
    emu::print_usage(argv[0]);
    return 2;
  }
  emu::log_level() = static_cast<emu::LogLevel>(cfg->log_level);

  std::unique_ptr<emu::PointSource> source;
  if (cfg->source == "lvx2") {
    auto lvx = std::make_unique<emu::Lvx2Source>();
    if (!lvx->open(cfg->lvx2_path, err)) {
      LOG_ERROR("%s", err.c_str());
      return 1;
    }
    source = std::move(lvx);
  } else {
    source = std::make_unique<emu::SyntheticSource>();
  }

  emu::PointStreamer points(*source, cfg->lidar_ip, cfg->batch);
  emu::ImuStreamer imu(cfg->lidar_ip);

  emu::ControlPlaneCallbacks cbs;
  cbs.on_point_target = [&](uint32_t ip, uint16_t port) { points.set_target(ip, port); };
  cbs.on_imu_target = [&](uint32_t ip, uint16_t port) { imu.set_target(ip, port); };

  emu::ControlPlane control(*cfg, std::move(cbs));
  if (!control.init(err)) {
    LOG_ERROR("%s", err.c_str());
    LOG_ERROR("hint: the emulator must run on a host owning %s (netns/guest)",
              cfg->lidar_ip.c_str());
    return 1;
  }

  std::signal(SIGINT, on_signal);
  std::signal(SIGTERM, on_signal);
  std::signal(SIGPIPE, SIG_IGN);

  LOG_INFO("livox mid-360 emulator: ip=%s sn=%s source=%s batch=%u",
           cfg->lidar_ip.c_str(), cfg->sn.c_str(), cfg->source.c_str(), cfg->batch);
  control.run(g_stop);

  points.stop();
  imu.stop();
  LOG_INFO("shutdown complete");
  return 0;
}
