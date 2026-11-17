cmake_minimum_required(VERSION 3.24)
project(LauncherTests LANGUAGES CXX)
enable_testing()
find_path(CVR_OPENXR_INCLUDE_DIR openxr/openxr.h HINTS
  "${CMAKE_CURRENT_SOURCE_DIR}/../../build/_deps/openxr-src/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../build-local/_deps/openxr-src/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/../../../_deps/openxr-src/include" REQUIRED)
add_executable(launcher_tests main.cpp ../../src/Overlay/LauncherDialog.cpp)
target_include_directories(launcher_tests PRIVATE ../../src/Lua/include ../../include ../../externals/minhook/include ${CVR_OPENXR_INCLUDE_DIR})
target_compile_features(launcher_tests PRIVATE cxx_std_20)
target_compile_definitions(launcher_tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX XR_USE_PLATFORM_WIN32 XR_USE_GRAPHICS_API_D3D12)
target_link_libraries(launcher_tests PRIVATE user32 gdi32 comctl32)
add_test(NAME launcher_window_and_input COMMAND launcher_tests)
set_tests_properties(launcher_window_and_input PROPERTIES TIMEOUT 15)
# Not a test: shows the launcher and saves what it looks like (preview.cpp).
add_executable(launcher_preview preview.cpp ../../src/Overlay/LauncherDialog.cpp)
target_include_directories(launcher_preview PRIVATE ../../include)
target_compile_features(launcher_preview PRIVATE cxx_std_20)
target_compile_definitions(launcher_preview PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(launcher_preview PRIVATE user32 gdi32 comctl32)
