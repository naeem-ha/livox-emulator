# Stock, unmodified livox_ros_driver2 on ROS2 Humble — the emulator's judge.
FROM ros:humble

RUN apt-get update && apt-get install -y --no-install-recommends \
      git cmake build-essential \
    && rm -rf /var/lib/apt/lists/*

# Livox-SDK2 (pinned release)
RUN git clone --depth 1 --branch v1.3.1 https://github.com/Livox-SDK/Livox-SDK2.git /tmp/sdk2 \
    && cmake -S /tmp/sdk2 -B /tmp/sdk2/build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/sdk2/build -j"$(nproc)" \
    && cmake --install /tmp/sdk2/build \
    && rm -rf /tmp/sdk2

# livox_ros_driver2 (pinned release), built unmodified via its own build.sh
RUN mkdir -p /ws_livox/src \
    && git clone --depth 1 --branch 1.2.6 https://github.com/Livox-SDK/livox_ros_driver2.git \
       /ws_livox/src/livox_ros_driver2
# The slim ros:humble base lacks PCL. rosdep can't see the ROS2 deps either:
# the repo ships a ROS1 package.xml and build.sh swaps in package_ROS2.xml only
# at build time — so install the ROS2 build deps explicitly.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
       libpcl-dev ros-humble-pcl-conversions \
    && rm -rf /var/lib/apt/lists/*
# build.sh does not propagate colcon failures; assert the real install outputs.
RUN cd /ws_livox/src/livox_ros_driver2 \
    && bash -c "source /opt/ros/humble/setup.bash && ./build.sh humble" \
    && find /ws_livox/install -name msg_MID360_launch.py | grep -q . \
    && find /ws_livox/install -name MID360_config.json | grep -q .

COPY judge-entry.sh /judge-entry.sh
RUN chmod +x /judge-entry.sh
ENTRYPOINT ["/judge-entry.sh"]
