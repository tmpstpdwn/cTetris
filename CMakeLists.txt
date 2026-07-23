cmake_minimum_required(VERSION 3.11)
project(cTetris VERSION 1.0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(EMSCRIPTEN)
    # raylib's bundled miniaudio uses EM_ASM, which requires a -std=gnu* mode.
    set(CMAKE_C_EXTENSIONS ON)
else()
    set(CMAKE_C_EXTENSIONS OFF)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_C_FLAGS_RELEASE "-O3 -flto")
if(EMSCRIPTEN)
    # `-s` (strip) is a settings prefix under emcc, not a strip flag.
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto")
else()
    set(CMAKE_EXE_LINKER_FLAGS_RELEASE "-flto -s")
endif()

add_compile_options(-Wall -Wextra -pedantic)

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(external/raylib)

file(GLOB SOURCES CONFIGURE_DEPENDS src/*.c)
add_executable(cTetris ${SOURCES})
target_include_directories(cTetris PRIVATE include)
target_link_libraries(cTetris PRIVATE raylib)

if(WIN32)
    enable_language(RC)
    target_sources(cTetris PRIVATE resource.rc)
    target_link_libraries(cTetris PRIVATE winmm)
    target_link_options(cTetris PRIVATE -Wl,--defsym=stat64i32=_stat64i32)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        set_target_properties(cTetris PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()

if(EMSCRIPTEN)
    # raylib already adds -sUSE_GLFW=3 to its public link options.
    # Output an .html page (plus .js/.wasm) using our own shell.
    set_target_properties(cTetris PROPERTIES SUFFIX ".html")
    target_link_options(cTetris PRIVATE
        -sALLOW_MEMORY_GROWTH=1
        -sASYNCIFY=0
        --shell-file "${CMAKE_SOURCE_DIR}/web/shell.html"
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT EMSCRIPTEN)
    target_compile_definitions(cTetris PRIVATE DEBUG)
    if(NOT MSVC)
        target_compile_options(cTetris PRIVATE -g -O0 -fno-omit-frame-pointer -fsanitize=address)
        target_link_options(cTetris PRIVATE -fsanitize=address)
    endif()
endif()
