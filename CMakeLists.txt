cmake_minimum_required(VERSION 3.12)
project(xbox_joystick_forwarder)

set(CMAKE_CXX_STANDARD 17)

# Find required packages

# Since we installed zenoh locally, we will add the path to our CMAKE_PREFIX_PATH so CMake can find it
list (APPEND CMAKE_PREFIX_PATH "~/.local")

# Find necessary packages
find_package(zenohc)
find_package(zenohcxx)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP REQUIRED jsoncpp)

# Set third_party paths
set(THIRD_PARTY_DIR ${CMAKE_SOURCE_DIR}/third_party)
set(ZMQ_ROOT ${THIRD_PARTY_DIR}/zeromq)
set(JSONCPP_ROOT ${THIRD_PARTY_DIR}/json)

# Include directories
include_directories(
    include
)

# Source files
set(SOURCES
    src/main.cpp
    src/xbox_controller.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    jsoncpp
    zenohcxx::zenohc
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} dinput8 dxguid xinput)
elseif(APPLE)
    find_library(IOKIT IOKit)
    find_library(COREFOUNDATION CoreFoundation)
    target_link_libraries(${PROJECT_NAME} ${IOKIT} ${COREFOUNDATION})
elseif(UNIX)
    target_link_libraries(${PROJECT_NAME} usb-1.0)
endif()