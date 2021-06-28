cmake_minimum_required(VERSION 3.1.0)

project(univalue VERSION 2.0 LANGUAGES CXX)

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

target_include_directories(univalue
    PUBLIC
        include
    PRIVATE
        lib
)

option(BUILD_TESTS "Build tests" ON)

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
endif(BUILD_TESTS)

