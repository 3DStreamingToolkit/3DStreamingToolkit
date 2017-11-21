Push-Location -Path .
$err = $null
$libCount = 0
$libTotal = 4

Set-Location -Path ($PSScriptRoot + "\..\Libraries\WebRTC")

try {
    & .\webrtcInstallLibs.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving WebRTC libraries: ' + $err.Message) -ForegroundColor Red
}

$libCount++
Write-Host 'Finished Library '$libCount'/'$libTotal
Set-Location -Path ($PSScriptRoot + "\..\Libraries\DirectXTK")

try {
    & .\InstallLibraries.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving DirectXTK libraries for native client: ' + $err.Message) -ForegroundColor Red
}

$libCount++
Write-Host 'Finished Library '$libCount'/'$libTotal
Set-Location -Path ($PSScriptRoot + "\..\Libraries\WebRTCUWP")

try {
    & .\InstallLibraries.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving WebRTC-UWP & LibYUV libraries for directx hololens client: ' + $err.Message) -ForegroundColor Red
}

$libCount++
Write-Host 'Finished Library '$libCount'/'$libTotal

Set-Location -Path ($PSScriptRoot)

try {
    & .\InstallLibraries.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving OpenGL libraries for OpenGL sample server: ' + $err.Message) -ForegroundColor Red
}

if ($err -eq $null) {
    Write-Host 'Libraries retrieved and up to date' -ForegroundColor Green
}

Pop-Location