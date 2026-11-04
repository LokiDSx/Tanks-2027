cmake_minimum_required(VERSION 3.16)
project(Tanks CXX)

if(VITA)
    set(TANKS_CONSOLE "PS Vita")
elseif(PSP)
    set(TANKS_CONSOLE "PSP")
elseif(ANDROID)
    set(TANKS_CONSOLE "Android")
else()
    message(FATAL_ERROR "This CMake project builds the PS Vita, PSP or Android port only. Use the Makefile for desktop builds.")
endif()

file(GLOB_RECURSE TANKS_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

if(ANDROID)
    # SDLActivity.java loads libSDL2.so and libmain.so and starts the game
    # by calling SDL_main() (src/main.cpp renames main() through SDL.h).
    add_library(main SHARED ${TANKS_SOURCES})
    set(TANKS_TARGET main)
else()
    add_executable(Tanks ${TANKS_SOURCES})
    set(TANKS_TARGET Tanks)
endif()

target_compile_features(${TANKS_TARGET} PRIVATE cxx_std_17)
target_compile_options(${TANKS_TARGET} PRIVATE -Wall -Wno-narrowing)

set(RESOURCES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/resources")

if(VITA)
    # -------------------------------------------------------------------------
    # PS Vita build (VPK)
    # -------------------------------------------------------------------------
    include("${VITASDK}/share/vita.cmake" REQUIRED)

    find_package(SDL2 REQUIRED CONFIG)
    find_package(SDL2_image REQUIRED CONFIG)
    find_package(SDL2_ttf REQUIRED CONFIG)
    find_package(SDL2_mixer REQUIRED CONFIG)

    target_compile_definitions(Tanks PRIVATE VITA_PORT CONSOLE_PORT)

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

    # -------------------------------------------------------------------------
    # Assets are packed into the VPK under assets/ and loaded at runtime from
    # app0:/assets/ (see AppConfig::assets_dir in src/appconfig.h).
    # -------------------------------------------------------------------------
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
elseif(ANDROID)
    # -------------------------------------------------------------------------
    # Android build (APK assembled by the Gradle project in android/)
    #
    # SDL2 and the SDL2_* satellite libraries are built from the sources in
    # android/deps as shared libraries and packaged into the APK by Gradle.
    # Our code becomes libmain.so, which SDLActivity.java loads right after
    # libSDL2.so and starts via SDL_main().
    # -------------------------------------------------------------------------
    enable_language(C)

    target_compile_definitions(${TANKS_TARGET} PRIVATE ANDROID_PORT CONSOLE_PORT)

    set(TANKS_DEPS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android/deps")

    # SDL2 as libSDL2.so; SDL2main is not needed (SDLActivity resolves
    # SDL_main in libmain.so directly).
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
    add_subdirectory("${TANKS_DEPS_DIR}/SDL2-2.32.4" sdl2-build)

    # SDL2_image: the PNG atlas loads through the bundled stb decoder, so no
    # external image codec libraries are required.
    set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
    set(SDL2IMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_TIFF OFF CACHE BOOL "" FORCE)
    set(SDL2IMAGE_WEBP OFF CACHE BOOL "" FORCE)
    add_subdirectory("${TANKS_DEPS_DIR}/SDL2_image-2.8.8" sdl2_image-build)

    # SDL2_ttf: freetype is vendored in external/.
    set(SDL2TTF_VENDORED ON CACHE BOOL "" FORCE)
    add_subdirectory("${TANKS_DEPS_DIR}/SDL2_ttf-2.24.0" sdl2_ttf-build)

    # SDL2_mixer: every sound effect is an .ogg file, decoded by the
    # bundled stb_vorbis; all other codecs are turned off so that none of
    # them try to locate system libraries.
    set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
    set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MP3 OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
    set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
    add_subdirectory("${TANKS_DEPS_DIR}/SDL2_mixer-2.8.1" sdl2_mixer-build)

    # The game includes SDL as <SDL2/SDL.h>, but the SDL sources lay the
    # headers out without the SDL2/ prefix; android/compat forwards to the
    # real headers through plain includes (that directory holds no plain
    # SDL.h, so the forwarders can never include themselves).
    target_include_directories(${TANKS_TARGET} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/android/compat")

    target_link_libraries(${TANKS_TARGET} PRIVATE
        SDL2::SDL2
        SDL2_image::SDL2_image
        SDL2_ttf::SDL2_ttf
        SDL2_mixer::SDL2_mixer
        android
        log)
else()
    # -------------------------------------------------------------------------
    # PSP build (EBOOT.PBP)
    #
    # SDL2, SDL2_image, SDL2_ttf, SDL2_mixer and every codec/image/font
    # dependency are provided by the pspdev SDK. The toolchain sets
    # PKG_CONFIG_EXECUTABLE to psp-pkg-config, which yields the full link line.
    # -------------------------------------------------------------------------
    target_compile_definitions(Tanks PRIVATE PSP_PORT CONSOLE_PORT)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(TANKS_SDL REQUIRED sdl2 SDL2_image SDL2_ttf SDL2_mixer)

    target_include_directories(Tanks PRIVATE ${TANKS_SDL_INCLUDE_DIRS})
    target_link_directories(Tanks PRIVATE ${TANKS_SDL_LIBRARY_DIRS})

    # Static PSP libraries have interdependencies, so they are wrapped in a
    # group to let the linker resolve them in any order.
    target_link_libraries(Tanks PRIVATE
        -Wl,--start-group
        ${TANKS_SDL_LIBRARIES}
        -Wl,--end-group)

    create_pbp_file(
        TARGET Tanks
        TITLE "Tanks 2027"
        VERSION 1.00
        ICON_PATH "${RESOURCES_DIR}/psp/icon0.png"
        BACKGROUND_PATH "${RESOURCES_DIR}/psp/background.png"
        PREVIEW_PATH "${RESOURCES_DIR}/psp/preview.png"
    )

    # -------------------------------------------------------------------------
    # Assets are shipped as loose files next to EBOOT.PBP and loaded at runtime
    # from ms0:/PSP/GAME/TANKS/assets/ (see AppConfig::assets_dir).
    # -------------------------------------------------------------------------
    add_custom_command(TARGET Tanks POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tanks>/assets/stages"
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Tanks>/assets/sounds"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RESOURCES_DIR}/textures/texture.png" "$<TARGET_FILE_DIR:Tanks>/assets/texture.png"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${RESOURCES_DIR}/font/kongtext.ttf" "$<TARGET_FILE_DIR:Tanks>/assets/kongtext.ttf"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RESOURCES_DIR}/stages" "$<TARGET_FILE_DIR:Tanks>/assets/stages"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${RESOURCES_DIR}/sounds" "$<TARGET_FILE_DIR:Tanks>/assets/sounds"
        COMMENT "Copying assets next to EBOOT.PBP")
endif()
