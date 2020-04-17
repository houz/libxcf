cmake_minimum_required(VERSION 3.9...3.16)

if(${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()

set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

# Add a sensible build type default and warning because empty means no optimization and no debug info.
if(NOT CMAKE_BUILD_TYPE)
  message("WARNING: CMAKE_BUILD_TYPE is not defined!\n         Defaulting to CMAKE_BUILD_TYPE=Release. Use ccmake to set a proper value.")
  SET(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel." FORCE)
endif(NOT CMAKE_BUILD_TYPE)

include(GNUInstallDirs)
include(FeatureSummary)

project(xcf VERSION 0.1.0
            DESCRIPTION "A library for writing GIMP XCF files"
            LANGUAGES C)

if(WIN32)
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_LINK_WHAT_YOU_USE ON)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# disable compilation inside the source tree. the results are harder to clean up and make it easy to commit build results
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not permitted. Make a separate folder for building:\nmkdir build; cd build; cmake ..\nBefore that, remove the files already created:\nrm -rf CMakeCache.txt CMakeFiles")
endif(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)

# Set available configurations
set(CMAKE_CONFIGURATION_TYPES "Debug;Release;" CACHE STRING "" FORCE)

# Support folders in project
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "_")

find_package(ZLIB REQUIRED)

add_library(xcf STATIC xcf.c xcf.h xcf_names.c xcf_names.h)

set_property(TARGET xcf PROPERTY C_STANDARD 99)

target_compile_definitions(xcf PRIVATE _DEFAULT_SOURCE) # needed for htobe*()

option(FORCE_COLORED_OUTPUT "Show colored compiler output." ON)
if(${FORCE_COLORED_OUTPUT})
  if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    target_compile_options(xcf PRIVATE -fdiagnostics-color=always)
  elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(xcf PRIVATE -fcolor-diagnostics)
  endif()
endif()

if (MSVC)
  # warning level 4
  target_compile_options(xcf PRIVATE /W4)
else()
  # lots of warnings
  target_compile_options(xcf PRIVATE -Wall -Wextra -pedantic)
endif()

option(ENABLE_WERROR "Stop and fail the build, if a compiler warning is triggered." OFF)
if(ENABLE_WERROR)
  target_compile_options(xcf PRIVATE "-Werror")
endif()

target_link_libraries(xcf PUBLIC ZLIB::ZLIB)
target_link_libraries(xcf PUBLIC m)

target_include_directories(xcf PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

feature_summary(WHAT ALL)
