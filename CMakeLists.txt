cmake_minimum_required(VERSION 3.20)
project(single_hid_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json vendored under third_party (for DeviceProfile)
set(THIRD_PARTY_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
if (EXISTS "${THIRD_PARTY_INCLUDE}/nlohmann/json.hpp")
    message(STATUS "Found nlohmann/json at ${THIRD_PARTY_INCLUDE}")
else()
    message(WARNING "nlohmann/json not found at ${THIRD_PARTY_INCLUDE}")
endif()

add_executable(single_hid_monitor
    main.cpp
    hid_input_backend.cpp
    my_hid_adapter.cpp
    HidCapabilities.cpp
    DeviceEnumerator.cpp
    DeviceProfile.cpp
)

target_include_directories(single_hid_monitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${THIRD_PARTY_INCLUDE}
)

# HID parsing APIs
# hidsdi.h + hid.lib
# raw input APIs come from user32 which is linked by default on MSVC toolchains
if (WIN32)
    target_link_libraries(single_hid_monitor PRIVATE hid)
endif()
