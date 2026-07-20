$certSubject = "CN=STORM EDEN Development"
$cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq $certSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "Generating new self-signed certificate for STORM EDEN..."
    $cert = New-SelfSignedCertificate -Subject $certSubject -KeyUsage DigitalSignature -Type CodeSigningCert -FriendlyName "STORM EDEN Code Signing" -CertStoreLocation "Cert:\CurrentUser\My" -NotAfter (Get-Date).AddYears(10)
    
    # Add to Trusted Root Certification Authorities (Root) for the current user
    $rootStore = New-Object System.Security.Cryptography.X509Certificates.X509Store("Root", "CurrentUser")
    $rootStore.Open("ReadWrite")
    $rootStore.Add($cert)
    $rootStore.Close()
}

Write-Host "Signing binaries..."
$binaries = @(
    "E:\STORM EDEN\build\bin\Release\eden.exe",
    "E:\STORM EDEN\build\bin\Release\eden-cli.exe",
    "E:\STORM EDEN\build\bin\Release\eden-room.exe"
)

foreach ($bin in $binaries) {
    if (Test-Path $bin) {
        Write-Host "Signing: $bin"
        $status = Set-AuthenticodeSignature -FilePath $bin -Certificate $cert
        Write-Host "Status: $($status.StatusMessage)"
    } else {
        Write-Host "Warning: $bin not found."
    }
}
