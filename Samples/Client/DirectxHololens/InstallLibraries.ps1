Add-Type -AssemblyName System.IO.Compression.FileSystem

function DecompressZip {    
    if((Test-Path ($PSScriptRoot + "\DirectxClientComponent\VideoDecoder\libyuv\libs")) -eq $false) {
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($PSScriptRoot + "\DirectxClientComponent\VideoDecoder\libyuv\libs.zip", $PSScriptRoot + "\DirectxClientComponent\VideoDecoder\libyuv\")
        Write-Host "Finished"
    }

    if((Test-Path ($PSScriptRoot + "\Org.WebRTC\x64")) -eq $false) {
        if((Test-Path ($PSScriptRoot + "\Org.WebRTC\Org.WebRtc.zip")) -eq $false) {
            New-Item -ItemType file ($PSScriptRoot + "\Org.WebRTC\Org.WebRtc.zip")
            Get-Content ($PSScriptRoot + "\Org.WebRTC\Org.WebRtc.zip.001"),($PSScriptRoot + "\Org.WebRTC\Org.WebRtc.zip.002") -Encoding Byte -ReadCount 512 | 
                Set-Content ($PSScriptRoot + "\Org.WebRTC\Org.WebRtc.zip") -Enc Byte
        }
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory(($PSScriptRoot + "\Org.WebRTC\Org.WebRTC.zip"), $PSScriptRoot + "\Org.WebRTC\")
        Write-Host "Finished"
    }
}

DecompressZip 
