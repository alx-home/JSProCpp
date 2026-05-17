cmake_minimum_required(VERSION 3.14)

project(promise_tests)

# Enable testing
include(CTest)
enable_testing()

# Add Catch2 (header-only, via FetchContent)
include(FetchContent)
FetchContent_Declare(
   Catch2
   GIT_REPOSITORY https://github.com/catchorg/Catch2.git
   GIT_TAG v3.5.2
)
FetchContent_MakeAvailable(Catch2)

# Test executable
file(GLOB_RECURSE PROMISE_TEST_SOURCES CONFIGURE_DEPENDS
   "${CMAKE_CURRENT_SOURCE_DIR}/Promise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/Resolver/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/CVPromise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/StatePromise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/MessageQueue/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/Pool/*.cpp"
)

add_executable(promise_test ${PROMISE_TEST_SOURCES})

target_include_directories(promise_test PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(promise_test PRIVATE
   alx-home::promise
   Catch2::Catch2WithMain
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   target_compile_options(promise_test PRIVATE -Wno-c2y-extensions)
endif()

add_test(NAME promise_test COMMAND promise_test)
