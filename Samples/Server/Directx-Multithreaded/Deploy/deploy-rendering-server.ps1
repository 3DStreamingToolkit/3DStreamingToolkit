$gpuDriverVersion = "385.08" 
$gpuDriverSuffix = "-tesla-desktop-winserver2016-international-whql.exe"
$gpuDriverBaseUri = "http://us.download.nvidia.com/Windows/Quadro_Certified/"

# Expect that the contents of the zip will have an x86 and x64 folder at root, with exes contained within
$RelativePathToServerExe = "\x64"
$DestinationFolder = "C:\3Dtoolkit"
$BinariesFolder = $DestinationFolder + "\binaries\MultithreadedServer"
$BinariesZipPath = "https://3dtoolkitstorage.blob.core.windows.net/releases/"
$BinariesZip = "3DStreamingToolkit-latest-MultithreadedServer-Release.zip"
$ExeName = "MultithreadedServerService.exe"

if((Test-Path ($DestinationFolder)) -eq $false) {
    New-Item -Path $DestinationFolder -ItemType Directory -Force 
}

function Get-ScriptDirectory
{
  $Invocation = (Get-Variable MyInvocation -Scope 1).Value
  Split-Path $Invocation.MyCommand.Path
}

cd $DestinationFolder

$PathToExecutable = $BinariesFolder+$RelativePathToServerExe

if((Test-Path ((Get-ScriptDirectory)+"\"+$BinariesZip)) -eq $false) {
    $client = new-object System.Net.WebClient
    $client.DownloadFile($BinariesZipPath+$BinariesZip,$BinariesZip)
}

Add-Type -A System.IO.Compression.FileSystem

if((Test-Path ($PathToExecutable + "\" + $ExeName)) -eq $false) {
    [IO.Compression.ZipFile]::ExtractToDirectory($BinariesZip, $BinariesFolder)
}

if((Test-Path ((Get-ScriptDirectory)+"\"+$gpuDriverVersion+$gpuDriverSuffix)) -eq $false) {
    $client = new-object System.Net.WebClient
    $client.DownloadFile(($gpuDriverBaseUri+$gpuDriverVersion+"/"+$gpuDriverVersion+$gpuDriverSuffix),($gpuDriverVersion+$gpuDriverSuffix))
}

cd (Get-ScriptDirectory)

& ((Get-ScriptDirectory)+"\"+$gpuDriverVersion+$gpuDriverSuffix) /s /noreboot /clean /noeula /nofinish /passive | Out-Null

cd (Split-Path -Path $PathToExecutable)

New-NetFirewallRule -DisplayName "3DStreamingServer" `
                    -Action Allow `
                    -Description "Ports Requireed for 3dstreaming" `
                    -Direction Inbound `
                    -Enabled True `
                    -EdgeTraversalPolicy Allow `
                    -Protocol TCP `
                    -LocalPort 80,443,3478,5349,19302 `
                    -Program ($PathToExecutable + "\" + $ExeName)

New-NetFirewallRule -DisplayName "3DStreamingServer" `
                    -Action Allow `
                    -Description "Ports Requireed for 3dstreaming" `
                    -Direction Inbound `
                    -Enabled True `
                    -EdgeTraversalPolicy Allow `
                    -Protocol UDP `
                    -LocalPort 80,443,3478,5349,19302 `
                    -Program ($PathToExecutable + "\" + $ExeName)

$service = Get-Service -Name "RenderService" -ErrorAction SilentlyContinue

if($service.Length -eq 0) {

    New-Service -Name "RenderService" -BinaryPathName ($PathToExecutable + "\" + $ExeName) -StartupType Automatic
    Start-Service -Name "RenderService"

    # Temporarily just booting the app
    # & ($PathToExecutable + "\" + $ExeName)
}

Restart-Computer