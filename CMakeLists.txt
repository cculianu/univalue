cmake_minimum_required(VERSION 3.1.0)

project(univalue VERSION 2.3.0 LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()

add_library(univalue SHARED
    lib/univalue.cpp
    lib/univalue_get.cpp
    lib/univalue_read.cpp
    lib/univalue_write.cpp
)

set_target_properties(univalue PROPERTIES
                      SOVERSION ${CMAKE_PROJECT_VERSION_MAJOR}.${CMAKE_PROJECT_VERSION_MINOR}.0
                      VERSION ${CMAKE_PROJECT_VERSION})

target_include_directories(univalue
    PUBLIC
        include
    PRIVATE
        lib
)

option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCH "Build benchmark test" ON)


if(BUILD_TESTS)
    add_custom_target(check)
    function(create_univalue_test NAME FILES)
        add_executable(${NAME} ${FILES})
        target_link_libraries(${NAME} univalue)
        add_custom_target(check-${NAME} ${NAME})
        add_dependencies(check check-${NAME})
    endfunction()

    create_univalue_test(unitester_test test/unitester.cpp)
    target_compile_definitions(unitester_test
        PUBLIC JSON_TEST_SRC="${PROJECT_SOURCE_DIR}/test"
    )

    create_univalue_test(object_test test/object.cpp)

    # This testing tool should not be run automatically since it expects input on stdin
    add_executable(json_test test/test_json.cpp)
    target_link_libraries(json_test univalue)

    # The basic example we provide. This should not be run automatically either.
    add_executable(basic_example examples/basic_example.cpp)
    target_link_libraries(basic_example univalue)

endif(BUILD_TESTS)


if(BUILD_BENCH)
    add_executable(bench_univalue bench/bench_univalue.cpp)
    target_link_libraries(bench_univalue PRIVATE univalue)

    # optionally, use nlohmann
    find_package(nlohmann_json)
    if(nlohmann_json_FOUND)
        target_link_libraries(bench_univalue PRIVATE nlohmann_json::nlohmann_json)
    else()
        message("nlohmann::json was not found, proceeding anyway since it is not required.")
        message("However, if you wish to use the \"bench\" target, please install it in")
        message("order to get a comparitive benchmark versus that library.")
        message("")
    endif()

    file(GLOB json_files LIST_DIRECTORIES false "${PROJECT_SOURCE_DIR}/bench/*.json")
    add_custom_target(bench
        COMMAND bench_univalue ${json_files}
        DEPENDS bench_univalue
    )
endif(BUILD_BENCH)

