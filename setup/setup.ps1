Push-Location -Path .
$err = $null

Set-Location -Path .\WebRTCLibs

try {
    & .\webrtcInstallLibs.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host 'Error retrieving WebRTC libraries: $err.Message' -ForegroundColor Red
}

Set-Location -Path ..
Set-Location -Path .\WebRTCNativeClient

try {
    & .\directXTKInstallLibs.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host 'Error retrieving WebRTC libraries: $err.Message' -ForegroundColor Red
}

if ($err -eq $null) {
    Write-Host 'Libraries retrieved and up to date' -ForegroundColor Green
}

Pop-Location