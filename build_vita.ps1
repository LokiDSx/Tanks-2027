# Builds the PS Vita port of Tanks (Tanks.vpk).
#
# Requirements:
#   - VitaSDK installed and the VITASDK environment variable pointing to it
#     (https://vitasdk.org). C:\vitasdk is auto-detected.
#   - Vita packages installed into the SDK:
#       sdl2, sdl2_image, sdl2_ttf (via vdpm)
#   - CMake >= 3.16 and Ninja.
#
# On the first run SDL2_mixer 2.8.1 (static; WAV + OGG via the bundled
# stb_vorbis, no external codec libraries needed) is built from source and
# installed into VitaSDK.

$ErrorActionPreference = "Stop"

if (-not $env:VITASDK) {
    if (Test-Path "C:\vitasdk") {
        $env:VITASDK = "C:\vitasdk"
    } else {
        throw "VITASDK environment variable is not set and C:\vitasdk was not found"
    }
}

$toolchain = Join-Path $env:VITASDK "share/vita.toolchain.cmake"
$mixerLib = Join-Path $env:VITASDK "arm-vita-eabi/lib/libSDL2_mixer.a"

if (-not (Test-Path $mixerLib)) {
    Write-Host "SDL2_mixer not found in VitaSDK, building it from source..."

    $thirdParty = Join-Path $PSScriptRoot "third_party"
    $mixerSrc = Join-Path $thirdParty "SDL_mixer"
    $mixerBuild = Join-Path $thirdParty "SDL_mixer-build-vita"

    if (-not (Test-Path (Join-Path $mixerSrc "CMakeLists.txt"))) {
        git clone --depth 1 --branch release-2.8.1 https://github.com/libsdl-org/SDL_mixer.git $mixerSrc
        if ($LASTEXITCODE -ne 0) { throw "Failed to clone SDL_mixer" }
    }

    # -fno-pic/-fno-pie is required: PIC code emits R_ARM_BASE_PREL relocs
    # (type 25) which vita-elf-create does not support.
    cmake -S $mixerSrc -B $mixerBuild -G Ninja `
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
        -DCMAKE_BUILD_TYPE=Release `
        -DBUILD_SHARED_LIBS=OFF `
        -DSDL2MIXER_INSTALL=ON `
        -DSDL2MIXER_SAMPLES=OFF `
        -DSDL2MIXER_CMD=OFF `
        -DSDL2MIXER_FLAC=OFF `
        -DSDL2MIXER_MOD=OFF `
        -DSDL2MIXER_MP3=OFF `
        -DSDL2MIXER_MIDI=OFF `
        -DSDL2MIXER_OPUS=OFF `
        -DSDL2MIXER_WAVPACK=OFF `
        -DSDL2MIXER_GME=OFF `
        -DSDL2MIXER_VORBIS=STB `
        -DSDL2MIXER_WAVE=ON `
        -DCMAKE_C_FLAGS="-fno-pic -fno-pie" `
        -DCMAKE_POSITION_INDEPENDENT_CODE=OFF
    if ($LASTEXITCODE -ne 0) { throw "SDL2_mixer configure failed" }

    cmake --build $mixerBuild
    if ($LASTEXITCODE -ne 0) { throw "SDL2_mixer build failed" }

    cmake --install $mixerBuild
    if ($LASTEXITCODE -ne 0) { throw "SDL2_mixer install failed" }

    Write-Host "SDL2_mixer installed into VitaSDK."
}

$buildDir = Join-Path $PSScriptRoot "build-vita"

cmake -S $PSScriptRoot -B $buildDir -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE="$toolchain" `
    -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

cmake --build $buildDir
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

$vpk = Join-Path $buildDir "Tanks.vpk"
if (Test-Path $vpk) {
    Write-Host ""
    Write-Host "VPK created: $vpk"
    Write-Host "Install it on a Vita with VitaShell, then launch from LiveArea."
} else {
    throw "VPK was not produced"
}
