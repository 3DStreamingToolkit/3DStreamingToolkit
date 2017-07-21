$gpuDriverVersion = "377.35" 
$gpuDriverSuffix = "-tesla-desktop-winserver2016-international-whql.exe"
$gpuDriverBaseUri = "http://us.download.nvidia.com/Windows/Quadro_Certified/"

# Expect that the contents of the zip will have an x86 and x64 folder at root, with exes contained within
$RelativePathToServerExe = "\x64"
$BinariesFolder = $DestinationFolder + "\binaries\SpinningCubeServer"
$BinariesZipPath = "https://3dtoolkitstorage.blob.core.windows.net/releases/"
$BinariesZip = "3DStreamingToolkit-20170717.17-SpinningCubeServer-Release.zip"
$ExeName = "SpinningCubeServerService.exe"

$DestinationFolder = "C:\3Dtoolkit"
if((Test-Path ($DestinationFolder)) -eq $false) {
    New-Item -Path $DestinationFolder -ItemType Directory -Force 
}

cd $DestinationFolder

$PathToExecutable = $BinariesFolder+$RelativePathToServerExe

if((Test-Path ($PSScriptRoot+"\"+$BinariesZip)) -eq $false) {
    $client = new-object System.Net.WebClient
    $client.DownloadFile($BinariesZipPath+$BinariesZip,$BinariesZip)
}

Add-Type -A System.IO.Compression.FileSystem

if((Test-Path ($PathToExecutable + "\" + $ExeName)) -eq $false) {
    [IO.Compression.ZipFile]::ExtractToDirectory($BinariesZip, $BinariesFolder)
}

if((Test-Path ($PSScriptRoot+"\"+$gpuDriverVersion+$gpuDriverSuffix)) -eq $false) {
    $client = new-object System.Net.WebClient
    $client.DownloadFile(($gpuDriverBaseUri+$gpuDriverVersion+"/"+$gpuDriverVersion+$gpuDriverSuffix),($gpuDriverVersion+$gpuDriverSuffix))

    & ($gpuDriverVersion+$gpuDriverSuffix) /s /noreboot /clean /noeula /nofinish /passive | Out-Null
}

cd (Split-Path -Path $PathToExecutable)

# $service = Get-Service -Name "RenderService" -ErrorAction SilentlyContinue

# if($service.Length -eq 0) {

    # Disabling service setup because app currently isn't registerested to system service callbacks
    # New-Service -Name "RenderService" -BinaryPathName ($PathToExecutable + "\" + $ExeName) -StartupType Automatic
    # Start-Service -Name "RenderService"

    # Temporarily just booting the app
    & ($PathToExecutable + "\" + $ExeName)
# }

# Restart-Computer