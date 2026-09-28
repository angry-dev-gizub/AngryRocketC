add_library(arc-sanitizers INTERFACE)

target_compile_options(
    arc-sanitizers
    INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
)

target_link_options(
    arc-sanitizers
    INTERFACE
    $<$<CONFIG:Debug>:-fsanitize=address,undefined>
)