. ../../Utilities/InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    # Get ETag header for current blob
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFileName = ($filename + $etag + ".zip")
    $localFullPath = ($PSScriptRoot + "\" + $localFileName)
    
    # Clear previous versions of the library
    Get-ChildItem -File -Path $PSScriptRoot -Filter ("*" + $filename + "*") | ForEach-Object { #
        if($_.Name -notmatch (".*" + $etag + ".*")) {
            Write-Host "Removing outdated lib"
            Remove-Item * -Include $_.Name
        }
    }

    # If the library with the current ETag does not exist 
    if ((Test-Path ($localFullPath)) -eq $false) {

        # Download the library
        Write-Host "Downloading $localFileName from $uri"
        Copy-File -SourcePath $uri -DestinationPath $localFullPath    
        Write-Host ("Downloaded " + $filename + " lib archive")

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
        
        # Clear the files from the previous library version
        if((Test-Path ($PSScriptRoot + $extractDir)) -eq $true) {
            Write-Host "Clearing existing $extractDir" 
            Remove-Item -Recurse -Force ($PSScriptRoot + $extractDir)
        }

        # Extract the latest library
        Write-Host "Extracting..."
        Expand-Archive -Path $localFullPath -DestinationPath $PSScriptRoot
        Write-Host "Finished"
    }
}

DecompressZip -filename "m62patch_nvpipe_headers"
DecompressZip -filename "m62patch_nvpipe_x64"
DecompressZip -filename "m62patch_nvpipe_Win32"
