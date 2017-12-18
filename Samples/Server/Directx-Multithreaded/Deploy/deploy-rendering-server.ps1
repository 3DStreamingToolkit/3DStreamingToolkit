if($args.Length -gt 1)
{
	$vmSize = "$($args[0])";
	$windowsOsVersion = "$($args[1])";
}
else
{
	$vmSize = "Standard_NV6";
	$windowsOsVersion = "2016-Datacenter-smalldisk";
}

if($vmSize.Contains("NV") -eq $true)
{
	if($windowsOsVersion.Contains("2016") -eq $true)
	{
		$gpuDriverURI = "https://gpudrivers.file.core.windows.net/nvinstance/Windows/385.41_grid_win10_server2016_64bit_international.exe?st=2017-11-08T07%3A44%3A00Z&se=2018-11-09T07%3A44%3A00Z&sp=rl&sv=2017-04-17&sr=s&sig=71eNNxrnzpI5sKpWA3bC9H8QfqNa1UowBhTaK9pivEY%3D"
	}
	else
	{
		$gpuDriverURI = "https://gpudrivers.file.core.windows.net/nvinstance/Windows/385.41_grid_win8_win7_server2012R2_server2008R2_64bit_international.exe?st=2017-11-07T23%3A44%3A00Z&se=2018-11-08T23%3A44%3A00Z&sp=rl&sv=2017-04-17&sr=s&sig=obSf0OAxdIxvQdDBBpwhE4FBpRFIAe1RbuJrLW9EtnI%3D"
	}
	$gpuDriverFileName = $gpuDriverURI.Substring($gpuDriverURI.IndexOf("Windows/") + 8, $gpuDriverURI.IndexOf("?") - $gpuDriverURI.IndexOf("Windows/") - 8)
} else {
	if($windowsOsVersion.Contains("2016") -eq $true)
	{
		$gpuDriverURI = "http://us.download.nvidia.com/Windows/Quadro_Certified/376.84/376.84-tesla-desktop-winserver2016-international-whql.exe"
	}
	else
	{
		$gpuDriverURI = "http://us.download.nvidia.com/Windows/Quadro_Certified/376.84/376.84-tesla-desktop-winserver2008-2012r2-64bit-international-whql.exe"
	}
	$gpuDriverFileName = $gpuDriverURI.Substring($gpuDriverURI.IndexOf("Quadro_Certified/") + 17)
}

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

if((Test-Path ((Get-ScriptDirectory)+"\"+$gpuDriverFileName)) -eq $false) {
    $client = new-object System.Net.WebClient
    $client.DownloadFile(($gpuDriverURI),($gpuDriverFileName))
}

cd (Get-ScriptDirectory)

& ((Get-ScriptDirectory)+"\"+$gpuDriverFileName) /s /noreboot /clean /noeula /nofinish /passive | Out-Null

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