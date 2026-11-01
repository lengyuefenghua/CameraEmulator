cmake_minimum_required(VERSION 3.25)
project(CameraEmulator VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8)
endif()

option(CAMERA_BUILD_TESTS "Build tests" ON)

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)

include(FetchContent)
if(WIN32)
    FetchContent_Declare(
        spdlog
        URL https://github.com/gabime/spdlog/archive/refs/tags/v1.15.3.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    set(SPDLOG_FMT_EXTERNAL OFF CACHE BOOL "Use bundled fmt with spdlog" FORCE)
    set(SPDLOG_BUILD_SHARED OFF CACHE BOOL "Build spdlog as a static dependency" FORCE)
    set(SPDLOG_BUILD_TESTS OFF CACHE BOOL "Disable spdlog tests" FORCE)
    set(SPDLOG_BUILD_EXAMPLE OFF CACHE BOOL "Disable spdlog examples" FORCE)
    FetchContent_MakeAvailable(spdlog)
else()
    FetchContent_Declare(
        Asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(Asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)
endif()

add_library(camera_core STATIC
    src/core/camera_model.cpp
    src/core/camera_profile_catalog.cpp
    src/core/register_map.cpp
    src/core/validation.cpp
)
target_include_directories(camera_core PUBLIC src)

add_library(camera_protocol STATIC
    src/protocol/ipv4_udp_packet.cpp
    src/protocol/ipv4_tcp_packet.cpp
    src/protocol/gvcp_codec.cpp
    src/protocol/genicam_xml_generator.cpp
    src/protocol/gvsp_codec.cpp
)
target_include_directories(camera_protocol PUBLIC src)
target_link_libraries(camera_protocol PUBLIC camera_core)

add_library(camera_engine STATIC
    src/engine/session_controller.cpp
    src/engine/gvcp_dispatcher.cpp
    src/engine/camera_instance.cpp
    src/engine/frame_pool.cpp
    src/engine/stream_controller.cpp
    src/engine/engine_facade.cpp
)
target_include_directories(camera_engine PUBLIC src)
target_compile_definitions(camera_engine PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(camera_engine PUBLIC camera_core camera_protocol)

if(WIN32)
    add_executable(camera_service_smoke tools/camera_service_smoke.cpp)
    target_compile_definitions(camera_service_smoke PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(camera_service_smoke PRIVATE camera_engine ws2_32)
endif()

if(WIN32)
    set(MVS_ROOT "C:/Program Files (x86)/MVS/Development" CACHE PATH "Hikvision MVS SDK development root")
    set(MVS_RUNTIME_DIR "C:/Program Files (x86)/Common Files/MVS/Runtime/Win64_x64" CACHE PATH "Hikvision MVS 64-bit runtime directory")
    if(CAMERA_BUILD_TESTS AND EXISTS "${MVS_ROOT}/Includes/MvCameraControl.h" AND EXISTS "${MVS_ROOT}/Libraries/win64/MvCameraControl.lib" AND EXISTS "${MVS_RUNTIME_DIR}/MvCameraControl.dll")
        add_executable(mvs_probe tools/mvs_probe.cpp)
        target_include_directories(mvs_probe PRIVATE "${MVS_ROOT}/Includes")
        target_link_directories(mvs_probe PRIVATE "${MVS_ROOT}/Libraries/win64")
        target_link_libraries(mvs_probe PRIVATE MvCameraControl)
        add_custom_command(TARGET mvs_probe POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MVS_RUNTIME_DIR}/MvCameraControl.dll" "$<TARGET_FILE_DIR:mvs_probe>/MvCameraControl.dll")
    endif()
endif()

if(WIN32)
    FetchContent_Declare(
        Asio
        URL https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-30-2.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(Asio)
    set(ASIO_INCLUDE_DIR ${asio_SOURCE_DIR}/asio/include)

    add_library(camera_http STATIC src/infra/http_xml_server.cpp)
    target_include_directories(camera_http PUBLIC src PRIVATE ${asio_SOURCE_DIR}/asio/include)
    target_compile_definitions(camera_http PUBLIC ASIO_STANDALONE)
    target_link_libraries(camera_http PUBLIC ws2_32 PRIVATE spdlog::spdlog)

    add_library(camera_udp_transport STATIC src/infra/win32/udp_pktinfo_transport.cpp)
    target_include_directories(camera_udp_transport PUBLIC src)
    target_link_libraries(camera_udp_transport PUBLIC ws2_32 iphlpapi)

    add_library(camera_wintun_peer STATIC src/infra/win32/wintun_peer.cpp src/infra/tunnel_http_server.cpp)
    target_include_directories(camera_wintun_peer PUBLIC src PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/wintun/include)
    target_link_libraries(camera_wintun_peer PUBLIC camera_core PRIVATE ws2_32 iphlpapi)

    add_library(camera_logging STATIC src/infra/logging.cpp)
    target_include_directories(camera_logging PUBLIC src)
    target_link_libraries(camera_logging PUBLIC spdlog::spdlog Qt6::Core)
    target_link_libraries(camera_wintun_peer PRIVATE camera_logging)
    target_link_libraries(camera_engine PUBLIC camera_wintun_peer camera_logging)

    add_executable(phase0_probe tools/phase0_probe.cpp)
    target_link_libraries(phase0_probe PRIVATE camera_udp_transport)

endif()

add_executable(camera_xml_dump src/app/xml_dump.cpp)
target_link_libraries(camera_xml_dump PRIVATE camera_protocol)

add_executable(camera_emulator WIN32
    src/app/qt/main.cpp
    src/app/qt/main_window.cpp
    src/app/qt/main_window.hpp
)
target_link_libraries(camera_emulator PRIVATE Qt6::Widgets camera_engine camera_protocol camera_logging)
target_compile_definitions(camera_emulator PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
if(MSVC)
    target_link_options(camera_emulator PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
endif()
add_custom_command(TARGET camera_emulator POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:camera_emulator>/virtual-cameras"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/virtual-cameras/example-camera.json" "$<TARGET_FILE_DIR:camera_emulator>/virtual-cameras/example-camera.json"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/wintun/bin/amd64/wintun.dll" "$<TARGET_FILE_DIR:camera_emulator>/wintun.dll")

if(MSVC)
    target_compile_options(camera_emulator PRIVATE /W4 /permissive- /utf-8)
    target_link_options(camera_emulator PRIVATE /DEBUG:FULL /MAP)
endif()

if(CAMERA_BUILD_TESTS)
    include(CTest)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    add_executable(camera_tests tests/unit/test_smoke.cpp tests/unit/test_gvcp_codec.cpp tests/unit/test_core.cpp tests/unit/test_camera_profile_catalog.cpp tests/unit/test_ipv4_udp_packet.cpp tests/unit/test_ipv4_tcp_packet.cpp tests/unit/test_tunnel_tcp.cpp tests/unit/test_session.cpp tests/unit/test_frame_pool.cpp tests/golden/test_gvcp_golden.cpp tests/golden/test_gvsp_golden.cpp tests/integration/test_stream_loopback.cpp tests/integration/test_gvcp_acquisition.cpp tests/integration/test_wintun_peer_loopback.cpp)
    target_include_directories(camera_tests PRIVATE src)
    target_compile_definitions(camera_tests PRIVATE CAMERA_TEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    target_link_libraries(camera_tests PRIVATE Catch2::Catch2WithMain camera_protocol camera_core camera_engine camera_wintun_peer camera_udp_transport)
    if(MSVC)
        target_compile_options(camera_tests PRIVATE /W4 /permissive- /utf-8)
    endif()
    include(Catch)
    catch_discover_tests(camera_tests)
endif()
