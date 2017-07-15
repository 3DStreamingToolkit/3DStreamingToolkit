Param(
  [string]$ServerType
)

if($ServerType -eq "multithread") {
    $RelativePathToServerExe = "MultiThreadedRendering"
    $ExeName = "MultithreadedServerService.exe"
} else {
    $RelativePathToServerExe = "SpinningCube"
    $ExeName = "SpinningCubeService.exe"
}
$DestinationFolder = "C:\3Dtoolkit"
mkdir $DestinationFolder

cd $DestinationFolder

$BinariesFolder = $DestinationFolder+'\binaries'+ (Get-Date -UFormat "%M%S")
$BinariesZipPath = "https://3dtoolkitstorage.blob.core.windows.net/releases/"
$BinariesZip = "3DStreamingToolkit-x64-Release-latest.zip"
$PathToExecutable = $BinariesFolder+$RelativePathToServerExe

$client = new-object System.Net.WebClient
$client.DownloadFile($BinariesZipPath,$BinariesZip)

Add-Type -A System.IO.Compression.FileSystem

[IO.Compression.ZipFile]::ExtractToDirectory($BinariesZip, $BinariesFolder)

$client = new-object System.Net.WebClient
$client.DownloadFile("http://us.download.nvidia.com/Windows/Quadro_Certified/377.35/","377.35-tesla-desktop-winserver2016-international-whql.exe")

& 377.35-tesla-desktop-winserver2016-international-whql.exe /s /noreboot /clean /noeula /nofinish /passive

cd (Split-Path -Path $PathToExecutable)

New-Service -Name "RenderService" -BinaryPathName ($PathToExecutable + "\" + $ExeName) -StartupType Automatic
Start-Service -Name "RenderService"

Restart-Computer