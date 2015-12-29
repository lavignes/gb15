set(SOURCE_DIR src)
set(HEADER_DIR include/gb15)

set(SOURCES
        ${SOURCE_DIR}/memmap.c
        ${SOURCE_DIR}/cpu.c
        ${SOURCE_DIR}/bios.c
        ${SOURCE_DIR}/lcd.c
)

set(HEADERS
        ${HEADER_DIR}/gb15.h
        ${HEADER_DIR}/types.h
        ${HEADER_DIR}/regfile.h
        ${HEADER_DIR}/state.h
        ${HEADER_DIR}/oam.h
        ${HEADER_DIR}/register.h
        ${HEADER_DIR}/memmap.h
        ${HEADER_DIR}/bios.h
        ${HEADER_DIR}/lcd.h
)

add_library(libgb15  ${SOURCES} ${HEADERS})
set_target_properties(libgb15 PROPERTIES OUTPUT_NAME gb15)
target_include_directories(libgb15 PUBLIC include)
