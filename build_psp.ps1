# Builds the PSP port of Tanks (EBOOT.PBP) by invoking build_psp.sh inside WSL.
#
# Requirements:
#   - WSL with a Linux distro (e.g. Ubuntu) — the official way to run the
#     pspdev SDK on Windows (https://pspdev.github.io).
#   - pspdev SDK installed inside WSL at ~/pspdev.
#
# Output: build-psp/ containing EBOOT.PBP and assets/. Copy the whole
# folder contents to ms0:/PSP/GAME/TANKS/ on the memory stick.

$ErrorActionPreference = "Stop"

# Convert this script's Windows directory to a WSL path (/mnt/<drive>/...)
$win = $PSScriptRoot -replace '\\', '/'
if ($win -match '^([A-Za-z]):(.*)$') {
    $wsl = "/mnt/" + $matches[1].ToLower() + $matches[2]
} else {
    throw "Could not convert path to WSL form: $PSScriptRoot"
}

$script = "$wsl/build_psp.sh"
Write-Host "Running $script in WSL..."
wsl -d Ubuntu -- bash "`"$script`""
if ($LASTEXITCODE -ne 0) {
    throw "PSP build failed (exit code $LASTEXITCODE)"
}
