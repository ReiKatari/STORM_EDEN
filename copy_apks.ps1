$mainline = 'e:\STORM EDEN 3\src\src\android\app\build\outputs\apk\mainline\release\app-mainline-release.apk'
$legacy = 'e:\STORM EDEN 3\src\src\android\app\build\outputs\apk\legacy\release\app-legacy-release.apk'
$sdk27 = 'e:\STORM EDEN 3\src\src\android\app\build\outputs\apk\sdk27\release\app-sdk27-release.apk'

Write-Host "Copying APKs with STORM_SWITCH_7.3.1 naming..."
if (Test-Path $mainline) {
    Copy-Item $mainline 'e:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1.apk' -Force
    Copy-Item $mainline 'e:\STORM EDEN 3\STORM_SWITCH_7.3.1.apk' -Force
}

if (Test-Path $legacy) {
    Copy-Item $legacy 'e:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1_LEGACY.apk' -Force
    Copy-Item $legacy 'e:\STORM EDEN 3\STORM_SWITCH_7.3.1_LEGACY.apk' -Force
}

if (Test-Path $sdk27) {
    Copy-Item $sdk27 'e:\STORM EDEN 3\Files\STORM_SWITCH_7.3.1_SDK27.apk' -Force
    Copy-Item $sdk27 'e:\STORM EDEN 3\STORM_SWITCH_7.3.1_SDK27.apk' -Force
}

Get-ChildItem -Path 'e:\STORM EDEN 3\Files' -Recurse | Unblock-File -ErrorAction SilentlyContinue
Get-ChildItem -Path 'e:\STORM EDEN 3\Assembling' -Recurse | Unblock-File -ErrorAction SilentlyContinue
Get-ChildItem -Path 'e:\STORM EDEN 3' -File | Unblock-File -ErrorAction SilentlyContinue

Write-Host "APKs copied and all files unblocked successfully!"
