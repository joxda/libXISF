cmake_minimum_required(VERSION 3.14)

project(libXISF LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

add_library(XISF SHARED
  libXISF_global.h
  libxisf.cpp
  libxisf.h
  lz4/lz4.c
  lz4/lz4.h
  lz4/lz4hc.c
  lz4/lz4hc.h
)

target_link_libraries(XISF PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_compile_definitions(XISF PRIVATE LIBXISF_LIBRARY)

set_target_properties(XISF PROPERTIES PUBLIC_HEADER libxisf.h)
