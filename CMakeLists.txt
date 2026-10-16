cmake_minimum_required(VERSION 3.20)
project(frame-mover CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# OpenVR headers and runtime library as shipped with SteamVR on the Frame.
set(OPENVR_HEADERS "/opt/steamvr/tools/hellovr_vulkan_linux/src/openvr/headers"
    CACHE PATH "Directory containing openvr.h")
set(OPENVR_LIB "/opt/steamvr/bin/linuxarm64/libopenvr_api.so"
    CACHE FILEPATH "libopenvr_api.so to link against")

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(VULKAN REQUIRED IMPORTED_TARGET vulkan)

# Pure model, no OpenVR: shared by the program and the unit tests.
add_library(playspace STATIC src/playspace.cpp)
target_include_directories(playspace PUBLIC src)

add_executable(frame-mover
  src/main.cpp
  src/actions.cpp
  src/app.cpp
  src/backup.cpp
  src/chaperone.cpp
  src/config.cpp
  src/install.cpp
  src/panel.cpp
  src/ui.cpp
  src/vk_texture.cpp)
target_include_directories(frame-mover PRIVATE src ${OPENVR_HEADERS})
target_compile_options(frame-mover PRIVATE -Wall -Wextra)
target_link_libraries(frame-mover PRIVATE playspace PkgConfig::CAIRO PkgConfig::VULKAN ${OPENVR_LIB} ${CMAKE_DL_LIBS} pthread)
# Vulkan create-info structs are conventionally brace-initialised with only sType set.
set_source_files_properties(src/vk_texture.cpp PROPERTIES COMPILE_OPTIONS -Wno-missing-field-initializers)
get_filename_component(OPENVR_LIB_DIR "${OPENVR_LIB}" DIRECTORY)
set_target_properties(frame-mover PROPERTIES
  BUILD_RPATH "${OPENVR_LIB_DIR}" INSTALL_RPATH "${OPENVR_LIB_DIR}")

# The program finds its action manifest and default bindings in a "data" directory next to it.
add_custom_command(TARGET frame-mover POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:frame-mover>/data)

# Unit tests (no SteamVR needed): ctest --test-dir build --output-on-failure
enable_testing()
add_executable(playspace_tests tests/playspace_tests.cpp)
target_link_libraries(playspace_tests PRIVATE playspace)
add_test(NAME playspace COMMAND playspace_tests)
