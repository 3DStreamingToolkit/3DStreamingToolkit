<#
.Synopsis
   This script is to transcode h264 video files encoded with nvencode into raw yuv420 files, then analyze them with VQMT and 
   export a single merged CSV for further data analysis.
.DESCRIPTION
   This script automates the conversion, analysis and merger of video files with VQMT.  It is currently configured to operate
   on h264 inputs, but will also work on any video format supported by ffmpeg.

   It requires the following naming convention on encoded videos to be processed:

   Input videos: 10000kbps-AQ1-lowLatencyHQ-CBR-LL-HQ.h264
                  5000kbps-AQ0-hq-VBRHQ.h264

                 bitrate+kbps (dash) adaptive quantization on/off (dash) encoderPreset (dash) CBR/VBR/VBRHQ designator
   There must also be one video labeled "lossless.ext" to use as the baseline for comparison.

   InputFolder      folder that will be searched, non-recursively, for videos.
   OutputFolder     location the merged CSV will be placed at completion of the script.
   CSVFilename      filename for the merged CSV
   videoFormat      extension for source video files to be analyzed 

   @Author: Tyler Gibson 2017


.\Transpose.ps1 -InputFile .\ProcessData.csv | Export-Csv ProcessDataT2.csv  -NoTypeInformation
#>
    [CmdletBinding()]
    [OutputType([PSCustomObject])]
    Param
    (
        #Custom input file name, default is "ProcessData.csv" in current directory
        [Parameter(Mandatory=$false,
                   Position=0)]
        $InputFolder = ".",
        [Parameter(Mandatory=$false,
                   Position=1)]
        $OutputFolder = ".\out",
        [Parameter(Mandatory=$false,
                   Position=2)]
        $CSVFileName = "fullDataSet.csv",
        [Parameter(Mandatory=$false,
                   Position=3)]
        $VideoFormat = "h264"

        
    )
    
Add-Type -AssemblyName System.IO.Compression.FileSystem

$outCSV = @()
$ffmpegPath = "ffmpeg-3.2.4-win64-static\bin"
if(-not (Test-Path $ffmpegPath) -Or ((Get-Command "ffmpeg.exe" -ErrorAction SilentlyContinue) -eq $null )) {
    $ffmpeg = Invoke-WebRequest -Uri https://ffmpeg.zeranoe.com/builds/win64/static/ffmpeg-3.2.4-win64-static.zip -OutFile $PWD\ffmpeg.zip
    [System.IO.Compression.ZipFile]::ExtractToDirectory("$PWD\ffmpeg.zip", ".\")
    Remove-Item -Path "ffmpeg.zip"
}
New-Item -ItemType Directory -Force -Path $OutputFolder
Start-Process -FilePath "$ffmpegPath\ffmpeg.exe" -ArgumentList "-i `"$InputFolder\lossless.$VideoFormat`" -c:v rawvideo -pix_fmt yuv420p `"$InputFolder\lossless.yuv`" -y" -Wait -NoNewWindow
Get-ChildItem $InputFolder -Filter *.$VideoFormat -File | 
Foreach-Object {
    if((Test-Path $_.FullName) -And (-NOT ($_.Name -eq "lossless.VideoFormat"))) {
        $video = $_
        Start-Process -FilePath "$ffmpegPath\ffmpeg.exe" -ArgumentList "-i `"$InputFolder\$video`" -c:v rawvideo -pix_fmt yuv420p `"$InputFolder\$video.yuv`" -y" -Wait -NoNewWindow
        Start-Process -FilePath "VQMT\VQMT.exe" -ArgumentList "`"$InputFolder\lossless.yuv`" `"$InputFolder\$video.yuv`" 720 2560 290 1 results PSNR SSIM MSSSIM VIFP PSNRHVS PSNRHVSM" -Wait -NoNewWindow
        Remove-Item -Path "$InputFolder\$video.yuv" -Force
        Get-ChildItem $PWD -Filter *.csv |
        ForEach-Object {
            $csv = $_;
            $pdata = @()
            if(-NOT ($csv.Name -eq $CSVFileName)) {
                $metadata = $video.Name.Split('.')[0].Split('-');
                $valueType = $csv.Name.Split('_')[1].Split('.')[0];
                $kbps = $metadata[0].Split('k')[0];
                $aq = $metadata[1];
                $type = $metadata[3];

                $content = Get-Content "$csv"
                $pdata = Import-Csv $csv.FullName |
                    Select-Object -Property frame, value |
                        Where-Object {-NOT ($_.frame -eq "average")}
                $pdata | Add-Member -MemberType NoteProperty "Kbps" -Value $kbps
                $pdata | Add-Member -MemberType NoteProperty "AQ" -Value $AQ
                $pdata | Add-Member -MemberType NoteProperty "Type" -Value $Type
                $pdata | Add-Member -MemberType NoteProperty "Test" -Value $valueType

                $pdata | Export-Csv -NoTypeInformation -Path $OutputFolder\$kbps$aq$valueType$type.csv
                
                $outCSV += $pdata |
                    Select-Object -Property Test, Kbps, AQ, Type, frame, value

                Remove-Item -Path "$InputFolder\$csv" -Force
            }
        }
    }
}
Remove-Item "$InputFolder\lossless.yuv" -Force
Remove-Item -Path ".\ffmpeg-3.2.4-win64-static" -Recurse
$outCSV | Export-Csv -NoTypeInformation -Path $OutputFolder\$CSVFileName    




 