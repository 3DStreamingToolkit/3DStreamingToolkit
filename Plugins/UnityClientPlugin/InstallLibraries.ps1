Add-Type -AssemblyName System.IO.Compression.FileSystem

function DecompressZip {
    if((Test-Path ($PSScriptRoot + "\TexturesUWP\libyuv\libs")) -eq $false) {
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($PSScriptRoot + "\TexturesUWP\libyuv\libs.zip", $PSScriptRoot + "\TexturesUWP\libyuv\")
        Write-Host "Finished"
    }
}

DecompressZip 