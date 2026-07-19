cmake_minimum_required(VERSION 3.16)
project(ScraplingCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(BUILD_TESTS "Build tests" ON)
option(BUILD_EXAMPLES "Build examples" ON)
option(ENABLE_CURL "Enable HTTP fetcher (requires libcurl)" ON)
option(ENABLE_WEBSOCKET "Enable CDP browser client (requires WebSocket++)" ON)

# Find dependencies
find_package(Threads REQUIRED)

# nlohmann/json (header-only, bundled)
include(FetchContent)
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(json)

# libcurl (optional)
if(ENABLE_CURL)
    find_package(CURL REQUIRED)
endif()

# WebSocket++ (optional, for CDP)
if(ENABLE_WEBSOCKET)
    find_package(Boost REQUIRED COMPONENTS system thread)
    FetchContent_Declare(
        websocketpp
        GIT_REPOSITORY https://github.com/zaphoyd/websocketpp.git
        GIT_TAG 0.8.2
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(websocketpp)
endif()

# Core library (parser + types, always built)
add_library(scrapling_core STATIC
    src/core/types.cpp
    src/parser/dom.cpp
    src/parser/css_selector.cpp
    src/parser/html_parser.cpp
    src/parser/selector.cpp
)

target_include_directories(scrapling_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(scrapling_core PUBLIC
    nlohmann_json::nlohmann_json
    Threads::Threads
)

# HTTP fetcher library (optional)
if(ENABLE_CURL)
    add_library(scrapling_fetcher STATIC
        src/fetcher/response.cpp
        src/fetcher/http_fetcher.cpp
    )
    target_include_directories(scrapling_fetcher PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(scrapling_fetcher PUBLIC
        scrapling_core
        CURL::libcurl
    )
endif()

# CDP browser engine (optional)
if(ENABLE_WEBSOCKET)
    add_library(scrapling_engine STATIC
        src/engines/cdp_client.cpp
    )
    target_include_directories(scrapling_engine PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<BUILD_INTERFACE:${websocketpp_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
    )
    target_link_libraries(scrapling_engine PUBLIC
        scrapling_fetcher
        Boost::system
        Boost::thread
    )
endif()

# Main library target
add_library(scrapling INTERFACE)
if(ENABLE_CURL AND ENABLE_WEBSOCKET)
    target_link_libraries(scrapling INTERFACE scrapling_engine)
elseif(ENABLE_CURL)
    target_link_libraries(scrapling INTERFACE scrapling_fetcher)
else()
    target_link_libraries(scrapling INTERFACE scrapling_core)
endif()

# Tests
if(BUILD_TESTS)
    add_executable(test_parser tests/test_parser.cpp)
    target_link_libraries(test_parser PRIVATE scrapling_core)

    if(ENABLE_CURL)
        add_executable(test_fetcher tests/test_fetcher.cpp)
        target_link_libraries(test_fetcher PRIVATE scrapling_fetcher)
    endif()

    if(ENABLE_WEBSOCKET)
        add_executable(test_cdp tests/test_cdp.cpp)
        target_link_libraries(test_cdp PRIVATE scrapling_engine)
    endif()

    enable_testing()
    add_test(NAME parser COMMAND test_parser)
    if(ENABLE_CURL)
        add_test(NAME fetcher COMMAND test_fetcher)
    endif()
endif()

# Examples
if(BUILD_EXAMPLES)
    add_executable(example_static examples/example_static.cpp)
    target_link_libraries(example_static PRIVATE scrapling)

    if(ENABLE_CURL)
        add_executable(example_fetch examples/example_fetch.cpp)
        target_link_libraries(example_fetch PRIVATE scrapling_fetcher)
    endif()
endif()

# Install
include(GNUInstallDirs)
install(TARGETS scrapling_core
    EXPORT scrapling-targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
if(ENABLE_CURL)
    install(TARGETS scrapling_fetcher
        EXPORT scrapling-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()
if(ENABLE_WEBSOCKET)
    install(TARGETS scrapling_engine
        EXPORT scrapling-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    )
endif()

install(EXPORT scrapling-targets
    FILE scrapling-targets.cmake
    NAMESPACE scrapling::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/scrapling
)

install(DIRECTORY src/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/scrapling
    FILES_MATCHING PATTERN "*.hpp"
)
