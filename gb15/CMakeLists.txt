set(SOURCE_DIR src)
set(HEADER_DIR include/gb15)

set(SOURCES
        ${SOURCE_DIR}/cpu.c
        ${SOURCE_DIR}/mmu.c
        ${SOURCE_DIR}/gpu.c
        ${SOURCE_DIR}/bios.c
        ${SOURCE_DIR}/util.c

        ${SOURCE_DIR}/util.h
)

set(HEADERS
        ${HEADER_DIR}/types.h
        ${HEADER_DIR}/gb15.h
        ${HEADER_DIR}/cpu.h
        ${HEADER_DIR}/mmu.h
        ${HEADER_DIR}/gpu.h
        ${HEADER_DIR}/bios.h
)

add_library(libgb15 ${SOURCES} ${HEADERS})
set_target_properties(libgb15 PROPERTIES OUTPUT_NAME gb15)
target_include_directories(libgb15 PUBLIC include)
