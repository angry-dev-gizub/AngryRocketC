#--------------------------------------------------------------------
# Arc-core static library
#--------------------------------------------------------------------
add_library(
    arc-core
    STATIC
    allocator/allocator.c
    allocator/allocators/system_allocator.c
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