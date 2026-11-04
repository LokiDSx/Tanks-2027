# Builds the Android port (debug APK) and installs it on a connected device.
#
# Requires: JDK 17, Android SDK + NDK + CMake (see android/), Gradle 8.10.2.
# The Gradle project lives in android/, but it drives the very same
# CMakeLists.txt that the Vita and PSP builds use.

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
& $gradle -p $project assembleDebug
if ($LASTEXITCODE -ne 0) {
    throw "Gradle build failed (exit code $LASTEXITCODE)"
}

$apk = Join-Path $project "app\build\outputs\apk\debug\app-debug.apk"
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
