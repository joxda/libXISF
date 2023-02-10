cmake_minimum_required(VERSION 3.14)

project(libXISF VERSION 0.1.2 LANGUAGES CXX C)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core REQUIRED)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)

add_library(XISF
  libXISF_global.h
  libxisf.cpp
  libxisf.h
  lz4/lz4.c
  lz4/lz4.h
  lz4/lz4hc.c
  lz4/lz4hc.h
)

set_target_properties(XISF PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

target_link_libraries(XISF PUBLIC Qt${QT_VERSION_MAJOR}::Core)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(XISF PRIVATE LIBXISF_LIBRARY)
    target_compile_definitions(XISF LIBXISF_SHARED_LIB)
endif(BUILD_SHARED_LIBS)

set(XISF_PUBLIC_HEADERS libxisf.h libXISF_global.h)

include(GNUInstallDirs)

install(FILES ${XISF_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS XISF LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

#testing

enable_testing()

add_executable(LibXISFTest
    test/main.cpp
    test/benchmark.cpp)

target_link_libraries(LibXISFTest XISF)

add_test(NAME LibXISFTest        COMMAND LibXISFTest)
add_test(NAME LibXISFTestRead    COMMAND LibXISFTest "${CMAKE_CURRENT_LIST_DIR}/test/test.xisf")
add_test(NAME LibXISFTestReadLZ4 COMMAND LibXISFTest "${CMAKE_CURRENT_LIST_DIR}/test/test_lz4.xisf")
