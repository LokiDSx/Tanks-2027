cmake_minimum_required(VERSION 3.16)
project(Tanks CXX)

if(NOT VITA)
    message(FATAL_ERROR "This CMake project builds the PS Vita port only. Use the Makefile for desktop builds.")
endif()

include("${VITASDK}/share/vita.cmake" REQUIRED)

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)
find_package(SDL2_mixer REQUIRED CONFIG)

file(GLOB_RECURSE TANKS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(Tanks ${TANKS_SOURCES})

target_compile_features(Tanks PRIVATE cxx_std_17)
target_compile_definitions(Tanks PRIVATE VITA_PORT)
target_compile_options(Tanks PRIVATE -Wall -Wno-narrowing)

# Static libraries on Vita need to be wrapped in a group: SDL2_image needs
# libpng/zlib, SDL2_ttf needs freetype, in no particular order.
target_link_libraries(Tanks
    -Wl,--start-group
    SDL2::SDL2
    SDL2_image::SDL2_image-static
    PNG::PNG
    SDL2_ttf::SDL2_ttf-static
    Freetype::Freetype
    SDL2_mixer::SDL2_mixer-static
    z
    bz2
    stdc++
    -Wl,--end-group)

# ---------------------------------------------------------------------------
# Assets are packed into the VPK under assets/ and loaded at runtime from
# app0:/assets/ (see AppConfig::assets_dir in src/appconfig.h).
# ---------------------------------------------------------------------------
set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

file(GLOB STAGE_FILES "${RESOURCES_DIR}/stages/*")
file(GLOB SOUND_FILES "${RESOURCES_DIR}/sounds/*.ogg")

set(VPK_ASSETS
    FILE "${RESOURCES_DIR}/textures/texture.png" assets/texture.png
    FILE "${RESOURCES_DIR}/font/kongtext.ttf" assets/kongtext.ttf
    FILE "${RESOURCES_DIR}/font/license.txt" assets/font-license.txt
)

foreach(stage_file ${STAGE_FILES})
    get_filename_component(stage_name "${stage_file}" NAME)
    list(APPEND VPK_ASSETS FILE "${stage_file}" "assets/stages/${stage_name}")
endforeach()

foreach(sound_file ${SOUND_FILES})
    get_filename_component(sound_name "${sound_file}" NAME)
    list(APPEND VPK_ASSETS FILE "${sound_file}" "assets/sounds/${sound_name}")
endforeach()

vita_create_self(Tanks-eboot Tanks)

vita_create_vpk(Tanks.vpk TANK00001 Tanks-eboot
    VERSION 01.00
    NAME "Tanks 2027"
    ${VPK_ASSETS}
    FILE sce_sys/icon0.png sce_sys/icon0.png
    FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
    FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
    FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
)
