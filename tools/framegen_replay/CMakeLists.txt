cmake_minimum_required(VERSION 3.20)
project(framegen_replay LANGUAGES CXX)
set(OPENXR_INCLUDE "" CACHE PATH "OpenXR headers")
set(FFX_SDK "" CACHE PATH "Pinned production FidelityFX sdk")
set(V557_SOURCE "" CACHE PATH "Known-good V557 source tree")
set(V557_BUILD "" CACHE PATH "Matching V557 build with static libraries")
add_executable(framegen_replay main.cpp ../../src/Framegen/Generator.cpp ../../src/Framegen/SourceTimeline.cpp
 ../../src/Framegen/SynthesisQueue.cpp ../../src/Framegen/DlssFrameGen.cpp ../../src/Framegen/InputCopy.cpp ../../src/Render/StereoDepth.cpp)
target_compile_features(framegen_replay PRIVATE cxx_std_20)
target_compile_definitions(framegen_replay PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN CVR_FRAMEGEN_TEST_RECEIPTS CVR_V557_REFERENCE)
target_include_directories(framegen_replay PRIVATE ../../include "${OPENXR_INCLUDE}" ../../externals/nvidia-optical-flow "${FFX_SDK}/include" "${V557_SOURCE}/include")
target_link_libraries(framegen_replay PRIVATE d3d12 dxgi d3dcompiler windowscodecs ole32
 "${FFX_SDK}/bin/ffx_sdk/ffx_frameinterpolation_x64.lib"
 "${FFX_SDK}/bin/ffx_sdk/ffx_opticalflow_x64.lib" "${FFX_SDK}/bin/ffx_sdk/ffx_backend_dx12_x64.lib"
 "${V557_BUILD}/Release/xrfg_d3d12_history.lib" "${V557_BUILD}/Release/xrfg_core.lib" "${V557_BUILD}/Release/xrfg_bridge_flight_logger.lib")
