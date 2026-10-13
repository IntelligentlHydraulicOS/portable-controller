cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(portable_controller)

file(GLOB app_sources src/*.c)
target_sources(app PRIVATE ${app_sources})

set(LFS_IMAGE ${CMAKE_BINARY_DIR}/lfs.bin)
add_custom_command(
    OUTPUT ${LFS_IMAGE}
    COMMAND mklittlefs
    ARGS -c ${CMAKE_SOURCE_DIR}/fs_content
         -b 4096
         -p 16
         -s 0x82000
         ${LFS_IMAGE}
    DEPENDS ${CMAKE_SOURCE_DIR}/fs_content
    COMMENT "Generating LittleFS image (340KB)..."
)
add_custom_target(lfs_image ALL DEPENDS ${LFS_IMAGE})