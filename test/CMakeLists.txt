cmake_minimum_required(VERSION 3.14)

project(JSProCpp_tests)

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
file(GLOB_RECURSE JSPROCPP_TEST_SOURCES CONFIGURE_DEPENDS
   "${CMAKE_CURRENT_SOURCE_DIR}/Promise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/Resolver/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/CVPromise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/StatePromise/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/MessageQueue/*.cpp"
   "${CMAKE_CURRENT_SOURCE_DIR}/Pool/*.cpp"
)

add_executable(JSProCpp_test ${JSPROCPP_TEST_SOURCES})

target_include_directories(JSProCpp_test PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(JSProCpp_test PRIVATE
   alx-home::jsprocpp
   Catch2::Catch2WithMain
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
   target_compile_options(JSProCpp_test PRIVATE -Wno-c2y-extensions)
endif()

add_test(NAME JSProCpp_test COMMAND JSProCpp_test)

if(JSPROCPP_MEMCHECK)
   target_compile_definitions(JSProCpp_test PUBLIC JSPROCPP_MEMCHECK)

   if(JSPROCPP_MEMCHECK_FULL)
      target_compile_definitions(JSProCpp_test PUBLIC JSPROCPP_MEMCHECK_FULL)
   endif(JSPROCPP_MEMCHECK_FULL)
endif(JSPROCPP_MEMCHECK)
