Add-Type -AssemblyName System.IO.Compression.FileSystem

function DecompressZip {    
    if((Test-Path ($PSScriptRoot + "\Win32")) -eq $false) {
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($PSScriptRoot + "\libs.zip", $PSScriptRoot)
        Write-Host "Finished"
    }
}

DecompressZip 
