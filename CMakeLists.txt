cmake_minimum_required(VERSION 3.16)
project(livox_mid360_emulator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(EMULATOR_STATIC "Link fully static (for QEMU guest / Raspberry Pi)" OFF)

add_compile_options(-Wall -Wextra)

add_library(livox_emu_core STATIC
  src/util/config.cpp
  src/protocol/crc.cpp
  src/protocol/frame_codec.cpp
  src/protocol/kv.cpp
  src/net/udp_socket.cpp
  src/control/control_plane.cpp
  src/data/pacer.cpp
  src/data/point_streamer.cpp
  src/data/imu_streamer.cpp
  src/source/synthetic_source.cpp
  src/source/lvx2_parser.cpp
  src/source/lvx2_source.cpp
)
target_include_directories(livox_emu_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(livox_emu_core PUBLIC Threads::Threads)

add_executable(livox_emulator src/main.cpp)
target_link_libraries(livox_emulator PRIVATE livox_emu_core)
if(EMULATOR_STATIC)
  target_link_options(livox_emulator PRIVATE -static)
endif()

enable_testing()
add_subdirectory(tests)
