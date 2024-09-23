cmake_minimum_required(VERSION 3.14)

project(libXISF VERSION 0.2.12 LANGUAGES CXX C
    HOMEPAGE_URL https://gitea.nouspiro.space/nou/libXISF
    DESCRIPTION "LibXISF is C++ library that can read and write XISF files produced by PixInsight.")

include(CMakeDependentOption)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(USE_BUNDLED_LIBS "Use bundled LZ4 PugiXML and Zlib. You can still exclude some" ON)
cmake_dependent_option(USE_BUNDLED_LZ4 "Use bundled LZ4" ON "USE_BUNDLED_LIBS" OFF)
cmake_dependent_option(USE_BUNDLED_PUGIXML "Use bundled PugiXML" ON "USE_BUNDLED_LIBS" OFF)
cmake_dependent_option(USE_BUNDLED_ZLIB "Use bundled Zlib" ON "USE_BUNDLED_LIBS" OFF)

find_package(PkgConfig)

if(USE_BUNDLED_LZ4)
    list(APPEND THIRD_PARTY_SRC
        lz4/lz4.c
        lz4/lz4.h
        lz4/lz4hc.c
        lz4/lz4hc.h)
    list(APPEND THIRD_PARTY_INCLUDE lz4)
else(USE_BUNDLED_LZ4)
    pkg_check_modules(LZ4 liblz4 IMPORTED_TARGET REQUIRED)
endif(USE_BUNDLED_LZ4)
if(USE_BUNDLED_PUGIXML)
    list(APPEND THIRD_PARTY_SRC pugixml/pugixml.cpp)
    list(APPEND THIRD_PARTY_INCLUDE pugixml)
else(USE_BUNDLED_PUGIXML)
    pkg_check_modules(PUGIXML pugixml IMPORTED_TARGET REQUIRED)
endif(USE_BUNDLED_PUGIXML)
if(USE_BUNDLED_ZLIB)
    list(APPEND THIRD_PARTY_SRC
        zlib/adler32.c
        zlib/compress.c
        zlib/crc32.c
        zlib/deflate.c
        zlib/gzclose.c
        zlib/gzlib.c
        zlib/gzread.c
        zlib/gzwrite.c
        zlib/inflate.c
        zlib/infback.c
        zlib/inftrees.c
        zlib/inffast.c
        zlib/trees.c
        zlib/uncompr.c
        zlib/zutil.c)
    set(ZLIB_PC ${CMAKE_CURRENT_BINARY_DIR}/zlib.pc)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib.pc.cmakein ${ZLIB_PC} @ONLY)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/zlib/zconf.h.cmakein ${CMAKE_CURRENT_BINARY_DIR}/zconf.h @ONLY)
    list(APPEND THIRD_PARTY_INCLUDE zlib)
else(USE_BUNDLED_ZLIB)
    pkg_check_modules(ZLIB zlib IMPORTED_TARGET REQUIRED)
endif(USE_BUNDLED_ZLIB)

add_library(XISF
  bytearray.cpp
  libXISF_global.h
  libxisf.cpp
  libxisf.h
  streambuffer.cpp
  streambuffer.h
  utils.cpp
  variant.cpp
  ${THIRD_PARTY_SRC}
)

if(USE_BUNDLED_LIBS)
    target_include_directories(XISF PRIVATE ${THIRD_PARTY_INCLUDE})
    if(NOT USE_BUNDLED_LZ4)
        target_link_libraries(XISF PUBLIC PkgConfig::LZ4)
        list(APPEND PC_LIBS_REQUIRE lz4)
    endif(NOT USE_BUNDLED_LZ4)
    if(NOT USE_BUNDLED_PUGIXML)
        target_link_libraries(XISF PUBLIC PkgConfig::PUGIXML)
        list(APPEND PC_LIBS_REQUIRE pugixml)
    endif(NOT USE_BUNDLED_PUGIXML)
    if(NOT USE_BUNDLED_ZLIB)
        target_link_libraries(XISF PUBLIC PkgConfig::ZLIB)
        list(APPEND PC_LIBS_REQUIRE zlib)
    endif(NOT USE_BUNDLED_ZLIB)
else(USE_BUNDLED_LIBS)
    target_link_libraries(XISF PUBLIC PkgConfig::LZ4 PkgConfig::PUGIXML PkgConfig::ZLIB)
    list(APPEND PC_LIBS_REQUIRE lz4 pugixml zlib)
endif(USE_BUNDLED_LIBS)

set_target_properties(XISF PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

pkg_check_modules(ZSTD libzstd IMPORTED_TARGET)
if(ZSTD_FOUND)
    target_compile_definitions(XISF PRIVATE HAVE_ZSTD)
    target_link_libraries(XISF PUBLIC PkgConfig::ZSTD)
endif(ZSTD_FOUND)

if(BUILD_SHARED_LIBS)
    target_compile_definitions(XISF PRIVATE LIBXISF_LIBRARY)
else(BUILD_SHARED_LIBS)
    target_compile_definitions(XISF PUBLIC LIBXISF_STATIC_LIB)
endif(BUILD_SHARED_LIBS)

set(XISF_PUBLIC_HEADERS libxisf.h libXISF_global.h)

include(GNUInstallDirs)

install(FILES ${XISF_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libxisf.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(TARGETS XISF LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})

list(JOIN PC_LIBS_REQUIRE ", " PC_REQUIRE_STR2)
if(NOT PC_REQUIRE_STR2 STREQUAL "")
    string(REPLACE "lz4" "liblz4" PC_REQUIRE_STR ${PC_REQUIRE_STR2})
endif()
list(TRANSFORM PC_LIBS_REQUIRE PREPEND "-l")
list(JOIN PC_LIBS_REQUIRE " " PC_LIBS_STR)
configure_file(libxisf.pc.in libxisf.pc @ONLY)

#testing

enable_testing()

add_executable(LibXISFTest
    test/main.cpp
    test/benchmark.cpp)

target_link_libraries(LibXISFTest XISF)

add_test(NAME LibXISFTest        COMMAND LibXISFTest)
add_test(NAME LibXISFTestRead    COMMAND LibXISFTest "${CMAKE_CURRENT_LIST_DIR}/test/test.xisf")
add_test(NAME LibXISFTestReadLZ4 COMMAND LibXISFTest "${CMAKE_CURRENT_LIST_DIR}/test/test_lz4.xisf")
