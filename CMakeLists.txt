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

# Source files organized by module
set(CORE_TYPES_SRCS
    core/types/hid_types.h
)

set(CORE_CAPABILITIES_SRCS
    core/capabilities/hid_capabilities.h
    core/capabilities/hid_capabilities.cpp
)

set(CORE_PROFILES_SRCS
    core/profiles/device_profile.h
    core/profiles/device_profile.cpp
    core/profiles/profile_serialization.h
    core/profiles/profile_serialization.cpp
)

set(PLATFORM_ENUMERATOR_SRCS
    platform/enumerator/device_enumerator.h
    platform/enumerator/device_enumerator.cpp
)

set(PLATFORM_RAW_INPUT_SRCS
    platform/raw_input/raw_input_window.h
    platform/raw_input/raw_input_window.cpp
    platform/raw_input/raw_input_registry.h
    platform/raw_input/raw_input_registry.cpp
    platform/raw_input/raw_input_parser.h
    platform/raw_input/raw_input_parser.cpp
)

set(PLATFORM_HID_SRCS
    platform/hid/hid_parser.h
    platform/hid/hid_parser.cpp
    platform/hid/hid_usage_names.h
    platform/hid/hid_usage_names.cpp
)

set(BACKEND_SRCS
    backend/hid_input_backend.h
    backend/hid_input_backend.cpp
    backend/device_manager.h
    backend/device_manager.cpp
    backend/state_publisher.h
    backend/state_publisher.cpp
    backend/auto_config.h
    backend/auto_config.cpp
)

set(ADAPTER_SRCS
    adapter/my_hid_adapter.h
    adapter/my_hid_adapter.cpp
    adapter/button_parser.h
    adapter/button_parser.cpp
    adapter/axis_parser.h
    adapter/axis_parser.cpp
    adapter/config_mapper.h
    adapter/config_mapper.cpp
)

set(CLI_COMMANDS_SRCS
    cli/commands/command.h
    cli/commands/list_cmd.h
    cli/commands/list_cmd.cpp
    cli/commands/select_cmd.h
    cli/commands/select_cmd.cpp
    cli/commands/print_cmd.h
    cli/commands/print_cmd.cpp
    cli/commands/write_cmd.h
    cli/commands/write_cmd.cpp
    cli/commands/quit_cmd.h
    cli/commands/quit_cmd.cpp
)

set(CLI_FORMATTERS_SRCS
    cli/formatters/device_formatter.h
    cli/formatters/device_formatter.cpp
    cli/formatters/config_formatter.h
    cli/formatters/config_formatter.cpp
    cli/formatters/caps_formatter.h
    cli/formatters/caps_formatter.cpp
    cli/formatters/live_formatter.h
    cli/formatters/live_formatter.cpp
)

set(CLI_MAIN_SRCS
    main.cpp
)



add_executable(single_hid_monitor
    ${CORE_TYPES_SRCS}
    ${CORE_CAPABILITIES_SRCS}
    ${CORE_PROFILES_SRCS}
    ${PLATFORM_ENUMERATOR_SRCS}
    ${PLATFORM_RAW_INPUT_SRCS}
    ${PLATFORM_HID_SRCS}
    ${BACKEND_SRCS}
    ${ADAPTER_SRCS}
    ${CLI_COMMANDS_SRCS}
    ${CLI_FORMATTERS_SRCS}
    ${CLI_MAIN_SRCS}
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

# IDE source groups (Visual Studio filters)
if (MSVC)
    source_group("core\\types" FILES ${CORE_TYPES_SRCS})
    source_group("core\\capabilities" FILES ${CORE_CAPABILITIES_SRCS})
    source_group("core\\profiles" FILES ${CORE_PROFILES_SRCS})
    source_group("platform\\enumerator" FILES ${PLATFORM_ENUMERATOR_SRCS})
    source_group("platform\\raw_input" FILES ${PLATFORM_RAW_INPUT_SRCS})
    source_group("platform\\hid" FILES ${PLATFORM_HID_SRCS})
    source_group("backend" FILES ${BACKEND_SRCS})
    source_group("adapter" FILES ${ADAPTER_SRCS})
    source_group("cli\\commands" FILES ${CLI_COMMANDS_SRCS})
    source_group("cli\\formatters" FILES ${CLI_FORMATTERS_SRCS})
    source_group("cli" FILES ${CLI_MAIN_SRCS})
endif()