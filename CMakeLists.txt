cmake_minimum_required(VERSION 3.20)

project(JSProCpp LANGUAGES CXX)

include(CMakeParseArguments)
include(FetchContent)
include(CTest)

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
  set(JSPROCPP_BUILD_TESTS_DEFAULT ON)
else()
  set(JSPROCPP_BUILD_TESTS_DEFAULT OFF)
endif()

option(JSPROCPP_BUILD_TESTS "Build the JSProCpp test executable" ${JSPROCPP_BUILD_TESTS_DEFAULT})
option(JSPROCPP_FETCH_BUILD_TOOLS "Fetch build_tools when win32 helper commands are unavailable" ON)
option(JSPROCPP_FETCH_CPP_UTILS "Fetch cpp_utils when alx-home::cpp_utils is unavailable" ON)

set(JSPROCPP_BUILD_TOOLS_GIT_REPOSITORY "https://github.com/alx-home/build_tools.git" CACHE STRING "Git repository used to fetch build_tools")
set(JSPROCPP_BUILD_TOOLS_GIT_TAG "release" CACHE STRING "Git tag or branch used to fetch build_tools")
set(JSPROCPP_CPP_UTILS_GIT_REPOSITORY "https://github.com/alx-home/cpp_utils.git" CACHE STRING "Git repository used to fetch cpp_utils")
set(JSPROCPP_CPP_UTILS_GIT_TAG "2de0a62a8165439114a8d676182233281b5e4f24" CACHE STRING "Git tag or branch used to fetch cpp_utils")

if(NOT COMMAND win32_library OR NOT COMMAND win32_executable)
  if(NOT JSPROCPP_FETCH_BUILD_TOOLS)
    message(FATAL_ERROR "win32 helper commands not found and JSPROCPP_FETCH_BUILD_TOOLS is OFF")
  endif()

  FetchContent_Declare(
    alx_home_build_tools
    GIT_REPOSITORY ${JSPROCPP_BUILD_TOOLS_GIT_REPOSITORY}
    GIT_TAG ${JSPROCPP_BUILD_TOOLS_GIT_TAG}
  )
  FetchContent_MakeAvailable(alx_home_build_tools)

  include(${alx_home_build_tools_SOURCE_DIR}/cmake/win32_library.cmake)
  include(${alx_home_build_tools_SOURCE_DIR}/cmake/win32_executable.cmake)
endif()

if(NOT TARGET alx-home::cpp_utils)
  if(NOT JSPROCPP_FETCH_CPP_UTILS)
    message(FATAL_ERROR "alx-home::cpp_utils target not found and JSPROCPP_FETCH_CPP_UTILS is OFF")
  endif()

  FetchContent_Declare(
    alx_home_cpp_utils
    GIT_REPOSITORY ${JSPROCPP_CPP_UTILS_GIT_REPOSITORY}
    GIT_TAG ${JSPROCPP_CPP_UTILS_GIT_TAG}
  )
  FetchContent_MakeAvailable(alx_home_cpp_utils)
endif()

option(JSPROCPP_MEMCHECK_RELEASE "Enable JSProCpp leak detection in release mode" OFF)
option(JSPROCPP_MEMCHECK_DEBUG "Enable JSProCpp leak detection in debug mode" ON)
option(JSPROCPP_MEMCHECK_FULL "Dump leaked JSProCpp promises" ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(JSPROCPP_MEMCHECK ${JSPROCPP_MEMCHECK_DEBUG})
else()
  set(JSPROCPP_MEMCHECK ${JSPROCPP_MEMCHECK_RELEASE})
endif()

win32_library(TARGET_NAME jsprocpp
    FILES
  src/Memcheck.cpp
  src/Reject.cpp
  src/Resolve.cpp
  src/Promise.cpp
  src/CVPromise.cpp
  src/Mutex.cpp
  src/StatePromise.cpp
  src/MessageQueue.cpp
)
add_library(alx-home::jsprocpp ALIAS jsprocpp)

target_compile_features(jsprocpp PUBLIC cxx_std_23)

target_include_directories(jsprocpp INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(jsprocpp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include/promise
)

target_link_libraries(jsprocpp PUBLIC alx-home::cpp_utils)

if(JSPROCPP_BUILD_TESTS)
  add_subdirectory(test)
endif()

if(JSPROCPP_MEMCHECK)
  target_compile_definitions(jsprocpp PUBLIC JSPROCPP_MEMCHECK)

  if(JSPROCPP_MEMCHECK_FULL)
    target_compile_definitions(jsprocpp PUBLIC JSPROCPP_MEMCHECK_FULL)
  endif(JSPROCPP_MEMCHECK_FULL)
endif(JSPROCPP_MEMCHECK)
