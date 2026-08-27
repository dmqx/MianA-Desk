cmake_minimum_required(VERSION 3.21)

include(../cmake/MianAVersion.cmake)
project(MianADeskPackage VERSION ${MIANA_VERSION} LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "The single-file package currently supports MSVC on Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(MIANA_DIST_DIR "" CACHE PATH "Deployed MianA Desk directory")
if(NOT EXISTS "${MIANA_DIST_DIR}/MianADesk.exe")
    message(FATAL_ERROR "Set MIANA_DIST_DIR to a deployed directory containing MianADesk.exe")
endif()

include(FetchContent)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DICTBUILDER OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    zstd
    URL https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
    URL_HASH SHA256=eb33e51f49a15e023950cd7825ca74a4a2b43db8354825ac24fc1b7ee09e6fa3
    SOURCE_SUBDIR build/cmake
)
FetchContent_MakeAvailable(zstd)
target_compile_options(libzstd_static PRIVATE $<$<CONFIG:Release>:/O2>)

add_executable(miana-packer packer.cpp)
target_link_libraries(miana-packer PRIVATE libzstd_static)
target_compile_options(miana-packer PRIVATE /W4 /permissive- /utf-8 /EHsc $<$<CONFIG:Release>:/O2>)
target_link_options(miana-packer PRIVATE $<$<CONFIG:Release>:/INCREMENTAL:NO>)

set(payload "${CMAKE_CURRENT_BINARY_DIR}/payload.zst")
# Re-pack whenever any file under MIANA_DIST_DIR changes, not only when the
# packer itself is rebuilt: the payload must always track the deployed runtime.
file(GLOB_RECURSE deployed_runtime CONFIGURE_DEPENDS "${MIANA_DIST_DIR}/*")
add_custom_command(
    OUTPUT "${payload}"
    COMMAND $<TARGET_FILE:miana-packer> "${MIANA_DIST_DIR}" "${payload}"
    DEPENDS miana-packer ${deployed_runtime}
    COMMENT "Packing the deployed runtime with Zstandard"
    VERBATIM
)
add_custom_target(miana-payload DEPENDS "${payload}")

file(TO_CMAKE_PATH "${CMAKE_CURRENT_LIST_DIR}/../assets/miana_desk.ico" package_icon)
file(TO_CMAKE_PATH "${payload}" package_payload)
configure_file(launcher.rc.in launcher.rc @ONLY)
configure_file(version.h.in version.h @ONLY)

add_executable(MianADeskPackage WIN32 launcher.cpp "${CMAKE_CURRENT_BINARY_DIR}/launcher.rc")
add_dependencies(MianADeskPackage miana-payload)
target_include_directories(MianADeskPackage PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_link_libraries(MianADeskPackage PRIVATE libzstd_static user32)
target_compile_definitions(MianADeskPackage PRIVATE UNICODE _UNICODE)
target_compile_options(MianADeskPackage PRIVATE /W4 /permissive- /utf-8 /EHsc $<$<CONFIG:Release>:/O2>)
target_link_options(MianADeskPackage PRIVATE
    $<$<CONFIG:Release>:/INCREMENTAL:NO>
    $<$<CONFIG:Release>:/OPT:REF>
    $<$<CONFIG:Release>:/OPT:ICF>
)
set_target_properties(MianADeskPackage PROPERTIES OUTPUT_NAME "MianA Desk")

install(TARGETS MianADeskPackage RUNTIME DESTINATION .)
