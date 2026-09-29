cmake_minimum_required(VERSION 3.16)
project(tiffview)

set(CMAKE_CXX_STANDARD 20)

option(TIFFVIEW_STATIC "Link statically (needs a static Qt in CMAKE_PREFIX_PATH)" OFF)

find_package(TIFF REQUIRED)
find_package(Threads REQUIRED)

if(TIFFVIEW_STATIC)
    # Resolve Qt's third-party dependencies (xcb, freetype, ...) to .a files.
    set(_saved_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a)

    # Qt's FindWrapZSTD prefers the zstd:: CMake targets, which are shared.
    # Skipping them makes it fall back to find_library, which now finds the .a.
    set(CMAKE_DISABLE_FIND_PACKAGE_zstd ON)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(tiffview
    src/main.cpp
    src/image_viewer.cpp
    src/slice_cache.cpp
    src/slice_cache_policy.cpp
    src/config.cpp
    src/colormap_lut.cpp
    src/colormap.cpp
    src/io/tiff/tiff_reader.cpp
)

target_link_libraries(tiffview PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Threads::Threads
)

if(TIFFVIEW_STATIC)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ${_saved_suffixes})
    target_include_directories(tiffview PRIVATE ${TIFF_INCLUDE_DIRS})
    target_link_options(tiffview PRIVATE -static-libstdc++ -static-libgcc)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        target_link_options(tiffview PRIVATE -s)
    endif()
    # Static archives resolve left to right, so libtiff, its codecs and the
    # libraries Qt's xcb/font stack leaves unresolved go last, in a group to
    # handle their circular references. glibc stays dynamic.
    set(_tail_libs
        tiff webp sharpyuv Lerc jbig zstd lzma jpeg deflate
        fontconfig freetype expat png16 bz2 brotlidec brotlicommon z
        X11-xcb X11 Xau Xdmcp xcb-util)
    list(TRANSFORM _tail_libs PREPEND "-l")
    string(JOIN " " _tail "-Wl,--start-group" "-Wl,-Bstatic" ${_tail_libs}
           "-Wl,-Bdynamic" "-Wl,--end-group")
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} ${_tail}")
else()
    target_link_libraries(tiffview PRIVATE TIFF::TIFF)
endif()

install(TARGETS tiffview
    RUNTIME DESTINATION $ENV{HOME}/.local/bin
)

# The tests link the shared libtiff, so they are skipped in static builds.
if(NOT TIFFVIEW_STATIC)
    enable_testing()
    add_subdirectory(tests)
endif()
