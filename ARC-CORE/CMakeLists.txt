#--------------------------------------------------------------------
# Arc-core static library
#--------------------------------------------------------------------
add_library(
    arc-core
    STATIC
    allocator/allocator.c
)

target_include_directories(
    arc-core
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(
    arc-core
    PRIVATE
    arc-sanitizers
)

target_link_options(
    arc-core
    PRIVATE
    arc-sanitizers
)