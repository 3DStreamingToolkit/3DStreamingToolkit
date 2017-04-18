Import-Module BitsTransfer
Add-Type -AssemblyName System.IO.Compression.FileSystem

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    if((Test-Path ($PSScriptRoot + "\" + $filename)) -eq $false) {
        Start-BitsTransfer -Source ($blobUri + $filename) -Destination ($PSScriptRoot + "\" + $filename)   
        Write-Host ("Downloaded " + $filename + " lib archive")
    } 

    $extractDir = ""

    if($filename -like "*x64*") {
        $extractDir = "\x64"
    } 
    if($filename -like "*Win32*") {
        $extractDir = "\Win32"
    }
    if($filename -like "*headers*") {
        $extractDir = "\headers"
    }
    if($extractDir -eq "") {
        return
    }

    if((Test-Path ($PSScriptRoot + $extractDir)) -eq $false) {
        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($PSScriptRoot + "\" + $filename, $PSScriptRoot)
        Write-Host "Finished"
    }
}

DecompressZip -filename "m58patch_x64.zip"
DecompressZip -filename "m58patch_Win32.zip"
DecompressZip -filename "m58patch_headers.zip"