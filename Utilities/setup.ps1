Push-Location -Path $PSScriptRoot

. .\InstallLibraries.ps1

$err = $null
$libCount = 0
$libTotal = 5

if (!(HasAzCopy)) 
{
    Write-Warning 'AzCopy not installed, downloads may take a long time'
}

Set-Location -Path ($PSScriptRoot + "\..\Libraries\Nvpipe")
try {
    & .\InstallLibraries.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving Nvpipe libraries: ' + $err.Message) -ForegroundColor Red
}

$libCount++
Write-Host 'Finished Library '$libCount'/'$libTotal
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
    & .\InstallOpenGL.ps1
} catch {
    $err = $_.Exception
}

if ($err) {
    Write-Host ('Error retrieving OpenGL libraries for OpenGL sample server: ' + $err.Message) -ForegroundColor Red
}

$libCount++
Write-Host 'Finished Library '$libCount'/'$libTotal

if ($err -eq $null) {
    Write-Host 'Libraries retrieved and up to date' -ForegroundColor Green
}

Pop-Location