#--------------------------------------------------------------------
# Generated files
#--------------------------------------------------------------------

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/version.h"
    @ONLY
)

#--------------------------------------------------------------------
# Arc-core static library
#--------------------------------------------------------------------
add_library(
    arc-core
    STATIC
    allocator/allocator.c
    result/result.c
    allocator/system_allocator.c
    collections/array.c
    collections/dlist.c
    collections/hashmap.c
    string/string.c
    string/stringview.c
    filesystem/filesystem.c
    filesystem/path.c
)

target_include_directories(
    arc-core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    arc-core
    PRIVATE
    arc-sanitizers
)