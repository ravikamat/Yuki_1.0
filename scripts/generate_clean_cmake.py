import os

# 1. Preamble & Configuration
preamble = """cmake_minimum_required(VERSION 3.20)

# Force dynamic MSVC runtime (/MD) early to propagate to FetchContent subprojects
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MD")
endif()

project(Yuki_1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # -- Parallel compilation: use half the logical cores to avoid RAM saturation -
    cmake_host_system_information(RESULT _cpu_count QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR _mp_count "${_cpu_count} / 2")
    if(_mp_count LESS 1)
        set(_mp_count 1)
    endif()
    add_compile_options(/MP${_mp_count} /Gm- /FS /Zc:char8_t- /MD)
    add_link_options(/FORCE:MULTIPLE)
    message(STATUS "MSVC parallel build: /MP${_mp_count} (${_cpu_count} logical cores, halved for RAM)")

    # Add Windows SDK library path for winhttp.lib etc.
    if(DEFINED ENV{WindowsSdkDir} AND DEFINED ENV{WindowsSDKVersion})
        link_directories("$ENV{WindowsSdkDir}\\\\Lib\\\\$ENV{WindowsSDKVersion}\\\\um\\\\x64")
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL" CACHE STRING "" FORCE)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MD")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MD")
endif()
"""

# 2. Include directories
subdirs = sorted([os.path.relpath(root, 'F:/Yuki_1.0').replace('\\', '/') for root, dirs, files in os.walk('F:/Yuki_1.0/src')])
inc_dirs = "include_directories(${CMAKE_SOURCE_DIR})\n" + "\n".join([f"include_directories(${{CMAKE_SOURCE_DIR}}/{d})" for d in subdirs])

# 3. Third-party FetchContent
third_party = """
# Preprocessor Definitions
add_compile_definitions(YNC_ENABLED=1)

include(FetchContent)

# -- llama.cpp (in-process model: generation + dense retrieval embedding) ------
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/llama_cpp-src/include/llama.h")
    set(FETCHCONTENT_SOURCE_DIR_LLAMA_CPP "${CMAKE_BINARY_DIR}/_deps/llama_cpp-src" CACHE PATH "" FORCE)
    message(STATUS "llama.cpp: using pre-cached source at ${CMAKE_BINARY_DIR}/_deps/llama_cpp-src")
endif()
FetchContent_Declare(
    llama_cpp
    GIT_REPOSITORY https://github.com/ggerganov/llama.cpp.git
    GIT_TAG        10bf611e533d81f739128304991c5e133c6aebd8
    CMAKE_ARGS
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
        -DCMAKE_CXX_FLAGS="/MD"
        -DCMAKE_C_FLAGS="/MD"
)
set(LLAMA_STATIC         ON CACHE BOOL "" FORCE)
set(LLAMA_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LLAMA_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS    OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(llama_cpp)
include_directories(BEFORE ${llama_cpp_SOURCE_DIR}/include)
include_directories(BEFORE ${llama_cpp_SOURCE_DIR}/ggml/include)

FetchContent_Declare(
    whisper
    GIT_REPOSITORY https://github.com/ggerganov/whisper.cpp.git
    GIT_TAG v1.5.4
    CMAKE_ARGS
        -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL
        -DCMAKE_CXX_FLAGS="/MD"
        -DCMAKE_C_FLAGS="/MD"
)
set(WHISPER_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(WHISPER_BUILD_SERVER   OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS      OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(whisper)

# -- nlohmann/json Header-Only Library ----------------------------------------
set(_NL_HEADER "${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include/nlohmann/json.hpp")
if(EXISTS "${_NL_HEADER}")
    message(STATUS "nlohmann_json: using pre-downloaded header at ${_NL_HEADER}")
    add_library(nlohmann_json INTERFACE)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
    target_include_directories(nlohmann_json INTERFACE
        "${CMAKE_BINARY_DIR}/_deps/nlohmann_json-src/include")
else()
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# Find CURL and Boost libraries (optional)
find_package(CURL QUIET)
if(CURL_FOUND)
    include_directories(${CURL_INCLUDE_DIRS})
    message(STATUS "CURL found: ${CURL_LIBRARIES}")
else()
    message(STATUS "CURL not found - SmartScraper HTTP calls will be disabled")
endif()
find_package(Boost QUIET COMPONENTS system thread)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    message(STATUS "Boost found")
else()
    message(STATUS "Boost not found - Boost-dependent features disabled")
endif()

# -- HNSWLib (Vector Database) ------------------------------------------------
if(EXISTS "${CMAKE_BINARY_DIR}/_deps/hnswlib-src/hnswlib/hnswlib.h")
    set(FETCHCONTENT_SOURCE_DIR_HNSWLIB "${CMAKE_BINARY_DIR}/_deps/hnswlib-src" CACHE PATH "" FORCE)
endif()
FetchContent_Declare(
    hnswlib
    GIT_REPOSITORY https://github.com/nmslib/hnswlib.git
    GIT_TAG v0.8.0
)
FetchContent_MakeAvailable(hnswlib)
include_directories(${hnswlib_SOURCE_DIR})
"""

# 4. All Core Sources
cpp_files = []
for root, dirs, files in os.walk('F:/Yuki_1.0/src'):
    for f in files:
        if f.endswith('.cpp'):
            rel = os.path.relpath(os.path.join(root, f), 'F:/Yuki_1.0').replace('\\', '/')
            if rel not in ['src/main.cpp', 'src/tools/migrate_to_swarm.cpp']:
                cpp_files.append(rel)
cpp_files.sort()

sources_block = "set(YUKI_CORE_SOURCES\n    " + "\n    ".join(cpp_files) + "\n)\n"

# 5. yuki_core Library & Main Target
core_target = """
# -- yuki_core static library -------------------------------------------------
add_library(yuki_core STATIC ${YUKI_CORE_SOURCES})
if(MSVC)
    set_source_files_properties(src/PresenceShell.cpp PROPERTIES COMPILE_FLAGS "/FIwindows.h /FIrichedit.h")
    target_compile_options(yuki_core PRIVATE /W4 /WX- /utf-8
        $<$<CONFIG:Release>:/O2 /Ob2>
        $<$<CONFIG:Debug>:/Od /RTC1>
    )
    target_link_libraries(yuki_core PUBLIC
        winhttp wininet psapi winmm user32 gdi32 dwmapi msimg32 gdiplus whisper ws2_32 dxgi bcrypt
        llama ggml ggml-base ggml-cpu
        nlohmann_json::nlohmann_json)
    if(CURL_FOUND)
        target_link_libraries(yuki_core PUBLIC CURL::libcurl)
        target_compile_definitions(yuki_core PRIVATE YUKI_HAS_CURL=1)
    endif()
    if(Boost_FOUND)
        target_link_libraries(yuki_core PUBLIC Boost::system Boost::thread)
    endif()
endif()

# -- Main executable ----------------------------------------------------------
add_executable(yuki src/main.cpp)
if(MSVC)
    target_compile_options(yuki PRIVATE /W4 /WX- /utf-8)
    target_link_libraries(yuki PRIVATE yuki_core)
endif()

# -- yuki-kb Knowledge Compiler CLI ------------------------------------------
add_executable(yuki_kb src/tools/yuki_kb.cpp)
target_link_libraries(yuki_kb PRIVATE yuki_core)

# -- Migrate tool executable --------------------------------------------------
add_executable(migrate_to_swarm src/tools/migrate_to_swarm.cpp)
target_link_libraries(migrate_to_swarm PRIVATE yuki_core)

add_custom_target(copy_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/data $<TARGET_FILE_DIR:yuki>/data
)

# Copy data folder at build time
file(COPY ${CMAKE_SOURCE_DIR}/data/ DESTINATION ${CMAKE_BINARY_DIR}/data FILES_MATCHING PATTERN "*.txt" PATTERN "*.json" PATTERN "*.ini" PATTERN "*.sql" PATTERN "*.jsonl")
"""

full_cmake = preamble + "\n" + inc_dirs + "\n" + third_party + "\n" + sources_block + "\n" + core_target

with open('F:/Yuki_1.0/CMakeLists.txt', 'w', encoding='utf-8') as f:
    f.write(full_cmake)

print("Clean CMakeLists.txt generated successfully!")
