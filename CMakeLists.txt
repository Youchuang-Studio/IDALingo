cmake_minimum_required(VERSION 3.24)

project(IDALingo VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)

option(IDALINGO_BUILD_EXPORTER "Build the text-collection variant" ON)
option(IDALINGO_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
set(IDALINGO_LANGUAGE_FILE "${PROJECT_SOURCE_DIR}/translations/zh-CN/language.txt"
  CACHE FILEPATH "Translation rules shipped as language.txt")
set(IDALINGO_QT_DIR "" CACHE PATH "Directory containing IDA's Linux Qt 6 libraries")
set(IDALINGO_IDA_EXECUTABLE "ida" CACHE FILEPATH "Default IDA executable used by the Linux launcher")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(IDALingoHelpers)

if(WIN32)
  if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The Windows build requires the 64-bit MSVC toolchain.")
  endif()

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  add_subdirectory(third_party/minhook EXCLUDE_FROM_ALL)
elseif(NOT UNIX OR APPLE)
  message(FATAL_ERROR "IDALingo currently supports Windows and Linux.")
endif()

add_subdirectory(src)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
