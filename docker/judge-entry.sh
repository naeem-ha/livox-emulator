#!/bin/bash
# Overlay the harness MID360_config.json (mounted at /config) onto the stock
# driver's installed config, then launch it. Extra args go to ros2 launch.
set -e
CFG=/ws_livox/install/livox_ros_driver2/share/livox_ros_driver2/config/MID360_config.json
if [ -f /config/MID360_config.json ]; then
  cp /config/MID360_config.json "$CFG"
  echo "judge: using harness MID360_config.json"
fi
source /opt/ros/humble/setup.bash
source /ws_livox/install/setup.bash
exec ros2 launch livox_ros_driver2 msg_MID360_launch.py "$@"
