$mainline = 'e:\STORM EDEN 3\src\src\android\app\build\outputs\apk\mainline\release\app-mainline-release.apk'
$legacy = 'e:\STORM EDEN 3\src\src\android\app\build\outputs\apk\legacy\release\app-legacy-release.apk'

Write-Host "Copying APKs with STORM_SWITCH_7.2.2 and STORM_EDEN_7.2.2 naming..."
Copy-Item $mainline 'e:\STORM EDEN 3\Files\STORM_SWITCH_7.2.2.apk' -Force
Copy-Item $mainline 'e:\STORM EDEN 3\STORM_SWITCH_7.2.2.apk' -Force
Copy-Item $mainline 'e:\STORM EDEN 3\Files\STORM_EDEN_7.2.2.apk' -Force
Copy-Item $mainline 'e:\STORM EDEN 3\STORM_EDEN_7.2.2.apk' -Force

Copy-Item $legacy 'e:\STORM EDEN 3\Files\STORM_SWITCH_7.2.2_LEGACY.apk' -Force
Copy-Item $legacy 'e:\STORM EDEN 3\STORM_SWITCH_7.2.2_LEGACY.apk' -Force
Copy-Item $legacy 'e:\STORM EDEN 3\Files\STORM_EDEN_7.2.2_LEGACY.apk' -Force
Copy-Item $legacy 'e:\STORM EDEN 3\STORM_EDEN_7.2.2_LEGACY.apk' -Force

Get-ChildItem -Path 'e:\STORM EDEN 3\Files' -Recurse | Unblock-File -ErrorAction SilentlyContinue
Get-ChildItem -Path 'e:\STORM EDEN 3\Assembling' -Recurse | Unblock-File -ErrorAction SilentlyContinue
Get-ChildItem -Path 'e:\STORM EDEN 3' -File | Unblock-File -ErrorAction SilentlyContinue

Write-Host "APKs copied and all files unblocked successfully!"
