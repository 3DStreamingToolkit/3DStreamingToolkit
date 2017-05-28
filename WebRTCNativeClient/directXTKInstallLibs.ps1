Add-Type -AssemblyName System.IO.Compression.FileSystem

function DecompressZip {    
    if((Test-Path ($PSScriptRoot + "\libs\Win32")) -eq $false) {
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($PSScriptRoot + "\libs\libs.zip", $PSScriptRoot + "\libs")
        Write-Host "Finished"
    }
}

DecompressZip 
