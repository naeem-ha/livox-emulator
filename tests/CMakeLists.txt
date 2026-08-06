add_executable(livox_emu_tests
  test_main.cpp
  test_crc.cpp
  test_frame_codec.cpp
  test_kv.cpp
  test_detection_golden.cpp
  test_lvx2_parser.cpp
  test_synthetic_source.cpp
)
target_include_directories(livox_emu_tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party)
target_link_libraries(livox_emu_tests PRIVATE livox_emu_core)
target_compile_definitions(livox_emu_tests
  PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
add_test(NAME unit COMMAND livox_emu_tests)
