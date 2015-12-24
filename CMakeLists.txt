cmake_minimum_required(VERSION 2.8)
project(gb15)

set(GB15_VERSION_MAJOR 0)
set(GB15_VERSION_MINOR 1)

# Add cmake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

# Set base CFLAGS
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99 -Wall -Wextra -Werror \
    -Wno-unused-result -Wno-unused-parameter -Wno-unused-function")

add_subdirectory(gb15)
add_subdirectory(frontend)

add_custom_target(uninstall
        "${CMAKE_COMMAND}" -P "${CMAKE_MODULE_PATH}/uninstall.cmake"
)