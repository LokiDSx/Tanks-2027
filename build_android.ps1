# Builds the Android port (debug APK by default, -Release for the signed
# store build) and installs it on a connected device.
#
# Requires: JDK 17, Android SDK + NDK + CMake (see android/), Gradle 8.10.2.
# The Gradle project lives in android/, but it drives the very same
# CMakeLists.txt that the Vita and PSP builds use.

param([switch]$Release)

$ErrorActionPreference = "Stop"

$javaHome = "C:\Program Files\Eclipse Adoptium\jdk-17.0.21.9-hotspot"
if (-not (Test-Path $javaHome)) {
    throw "JDK 17 not found at $javaHome"
}
$env:JAVA_HOME = $javaHome

$sdk = Join-Path $env:LOCALAPPDATA "Android\Sdk"
if (-not (Test-Path $sdk)) {
    throw "Android SDK not found at $sdk"
}
$env:ANDROID_HOME = $sdk

$gradle = Join-Path $env:LOCALAPPDATA "gradle\gradle-8.10.2\bin\gradle.bat"
if (-not (Test-Path $gradle)) {
    throw "Gradle not found at $gradle"
}

$project = Join-Path $PSScriptRoot "android"

# SDL2 sources are vendored as an extracted (gitignored) tree in android/deps.
# The touch-coordinates patch must survive any re-extraction from the tarball:
# apply it whenever the marker comment is missing from SDL_render.c.
$sdlSrc  = Join-Path $PSScriptRoot "android\deps\SDL2-2.32.4\src\render\SDL_render.c"
$sdlPat  = Join-Path $PSScriptRoot "android\patches\sdl_render_touch_noop.patch"
if (Test-Path $sdlSrc) {
    $sdlText = [IO.File]::ReadAllText($sdlSrc)
    if (-not $sdlText.Contains("TANKS-2027 PATCH")) {
        Write-Host "Applying SDL touch-coordinates patch..."
        git -C $PSScriptRoot apply --check $sdlPat
        if ($LASTEXITCODE -ne 0) { throw "SDL patch does not apply cleanly" }
        git -C $PSScriptRoot apply $sdlPat
        if ($LASTEXITCODE -ne 0) { throw "git apply failed" }
    }
}

if ($Release) {
    $task = "assembleRelease"
    $apkRel = "app\build\outputs\apk\release\app-release.apk"
} else {
    $task = "assembleDebug"
    $apkRel = "app\build\outputs\apk\debug\app-debug.apk"
}

& $gradle -p $project $task
if ($LASTEXITCODE -ne 0) {
    throw "Gradle build failed (exit code $LASTEXITCODE)"
}

$apk = Join-Path $project $apkRel
Write-Host ""
Write-Host "APK: $apk"

# Install on the connected device, if any.
$adb = Join-Path $sdk "platform-tools\adb.exe"
$devices = & $adb devices
if ($devices -match "\tdevice\r?$") {
    Write-Host "Device found, installing..."
    & $adb install -r $apk
} else {
    Write-Host "No device connected - copy the APK to the phone manually."
}
