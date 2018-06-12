. ../../Utilities/InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    # Get ETag header for current blob
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFileName = ($filename + $etag + ".zip")
    $localFullPath = ($PSScriptRoot + "\" + $localFileName)
    
    # If the library with the current ETag does not exist 
    if ((Test-Path ($localFullPath)) -eq $false) {

        # Clear the files from the previous library version
        Write-Host "Clearing the existing $filename library"
        Get-ChildItem -Path $PSScriptRoot | ForEach-Object {
            $scriptName = [System.IO.Path]::GetFileName($PSCommandPath)
            if ($_.Name -ne $localFileName -and $_.Name -ne $scriptName) {
                Remove-Item -Recurse -Force ($PSScriptRoot + "\" + $_.Name)
            }
        }

                # Download the library
        Write-Host "Downloading $filename from $uri"
        Copy-File -SourcePath $uri -DestinationPath $localFullPath    
        Write-Host ("Downloaded " + $filename + " lib archive")


        # Extract the latest library
        Write-Host "Extracting..."
        Expand-Archive -Path $localFullPath -DestinationPath $PSScriptRoot
        Write-Host "Finished"
    }
}

DecompressZip -filename "libDirectXTK"

