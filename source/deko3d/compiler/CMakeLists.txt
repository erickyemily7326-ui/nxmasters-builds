cmake_minimum_required(VERSION 3.22)
project(NetherSX2DekoDependencies LANGUAGES C CXX)
get_filename_component(NETHERSX2_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
option(DEKO3D_SHADER_TESTS "Build host shader and native API validation" OFF)
include(Uam.cmake)
if(DEKO3D_SHADER_TESTS)
  add_executable(deko3d_shader_validation
    "${NETHERSX2_ROOT}/tests/deko3d/shader_validation.cpp"
    "${NETHERSX2_ROOT}/source/deko3d/dk_shader_source.cpp")
  target_include_directories(deko3d_shader_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${CMAKE_CURRENT_SOURCE_DIR}"
    "${NETHERSX2_ROOT}/third_party/lsfg-deko3d/include")
  target_compile_features(deko3d_shader_validation PRIVATE cxx_std_20)
  target_link_libraries(deko3d_shader_validation PRIVATE uam)
  find_path(DEKO3D_INCLUDE_DIR deko3d.h HINTS "$ENV{DEVKITPRO}/libnx/include" REQUIRED)
  add_executable(deko3d_transfer_validation
    "${NETHERSX2_ROOT}/tests/deko3d/transfer_validation.cpp")
  target_include_directories(deko3d_transfer_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${DEKO3D_INCLUDE_DIR}")
  target_compile_features(deko3d_transfer_validation PRIVATE cxx_std_20)
  enable_testing()
  add_test(NAME deko3d_transfer_validation COMMAND deko3d_transfer_validation)
  add_executable(deko3d_render_target_validation
    "${NETHERSX2_ROOT}/tests/deko3d/render_target_validation.cpp")
  target_include_directories(deko3d_render_target_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${DEKO3D_INCLUDE_DIR}")
  target_compile_features(deko3d_render_target_validation PRIVATE cxx_std_20)
  add_test(NAME deko3d_render_target_validation COMMAND deko3d_render_target_validation)
  add_executable(deko3d_vertex_format_validation
    "${NETHERSX2_ROOT}/tests/deko3d/vertex_format_validation.cpp")
  target_include_directories(deko3d_vertex_format_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${DEKO3D_INCLUDE_DIR}")
  target_compile_features(deko3d_vertex_format_validation PRIVATE cxx_std_20)
  add_test(NAME deko3d_vertex_format_validation COMMAND deko3d_vertex_format_validation)
  add_executable(deko3d_render_state_validation
    "${NETHERSX2_ROOT}/tests/deko3d/render_state_validation.cpp")
  target_include_directories(deko3d_render_state_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${DEKO3D_INCLUDE_DIR}")
  target_compile_features(deko3d_render_state_validation PRIVATE cxx_std_20)
  add_test(NAME deko3d_render_state_validation COMMAND deko3d_render_state_validation)
  add_executable(deko3d_presentation_validation
    "${NETHERSX2_ROOT}/tests/deko3d/presentation_validation.cpp"
    "${NETHERSX2_ROOT}/source/deko3d/dk_presentation.cpp")
  target_include_directories(deko3d_presentation_validation PRIVATE
    "${NETHERSX2_ROOT}/source/deko3d" "${DEKO3D_INCLUDE_DIR}")
  target_compile_features(deko3d_presentation_validation PRIVATE cxx_std_20)
  add_test(NAME deko3d_presentation_validation COMMAND deko3d_presentation_validation)
endif()
include(FetchContent)
FetchContent_Declare(spirv_cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG ebe2aa0cd80f5eb5cd8a605da604cacf72205f3b)
FetchContent_GetProperties(spirv_cross)
if(NOT spirv_cross_POPULATED)
  FetchContent_Populate(spirv_cross)
endif()
set(LSFG_SPIRV_CROSS_ROOT "${spirv_cross_SOURCE_DIR}")
set(LSFG_UAM_BRIDGE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
add_subdirectory("${NETHERSX2_ROOT}/third_party/lsfg-deko3d" lsfg)
