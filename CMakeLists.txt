cmake_minimum_required(VERSION 3.20)
project(single_hid_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json from obs-deps (for DeviceProfile)
set(OBS_DEPS_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/obs-plugintemplate/.deps/obs-deps-2025-07-11-x64/include")
if (EXISTS "${OBS_DEPS_INCLUDE}/nlohmann/json.hpp")
    message(STATUS "Found nlohmann/json at ${OBS_DEPS_INCLUDE}")
else()
    message(WARNING "nlohmann/json not found at ${OBS_DEPS_INCLUDE}")
endif()

add_executable(single_hid_monitor
    main_standalone.cpp
    hid_input_backend.cpp
    my_hid_adapter.cpp
    DeviceEnumerator.cpp
    DeviceProfile.cpp
)

target_include_directories(single_hid_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OBS_DEPS_INCLUDE}
)

# HID parsing APIs
# hidsdi.h + hid.lib
# raw input APIs come from user32 which is linked by default on MSVC toolchains
if (WIN32)
    target_link_libraries(single_hid_monitor PRIVATE hid)
endif()
