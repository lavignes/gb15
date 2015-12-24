set(SOURCES
        gb15.c
)

find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

add_executable(gb15 ${SOURCES})

target_link_libraries(gb15 libgb15 ${SDL2_LIBRARY})
