#--------------------------------------------------------------------
# Includes
#--------------------------------------------------------------------
include(GoogleTest)
include("${CMAKE_CURRENT_SOURCE_DIR}/test_sources.cmake")

### for each source file, create an executable and link it to gtest and arc-core.

foreach(TEST_SOURCE IN LISTS ARC_TEST_SOURCES)

    get_filename_component(
        TEST_NAME
        "${TEST_SOURCE}"
        NAME_WE
    )

    string(
        REPLACE "_test"
        ""
        TEST_NAME
        "${TEST_NAME}"
    )

    set(TEST_TARGET "arc-${TEST_NAME}-tests")

    add_executable(
        ${TEST_TARGET}
        ${TEST_SOURCE}
    )

    target_link_libraries(
        ${TEST_TARGET}
        PRIVATE
        GTest::gtest_main
        arc-core
        arc-sanitizers
    )

    # c++ 17.
    target_compile_features(
        ${TEST_TARGET}
        PRIVATE
        cxx_std_17
    )

    gtest_discover_tests(
        ${TEST_TARGET}
    )

endforeach()