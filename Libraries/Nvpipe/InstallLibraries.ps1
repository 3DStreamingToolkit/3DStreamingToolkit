Add-Type -AssemblyName System.IO.Compression.FileSystem

. ../../Utilities/InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    # Get ETag header for current blob
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFullPath = ($PSScriptRoot + "\" + $filename + ".zip")
    
    # Compare ETag against the currently installed version
    $versionMatch = Compare-Version -Path $localFullPath -Version $etag
    if (!$versionMatch) {

        # Clear the files from the previous library version
        Write-Host "Clearing the existing $filename library"
        Get-ChildItem -Path $PSScriptRoot | ForEach-Object {
            $scriptName = [System.IO.Path]::GetFileName($PSCommandPath)
            # Do not remove the install script
            if ($_.Name -ne $scriptName) {
                Remove-Item -Recurse -Force ($PSScriptRoot + "\" + $_.Name)
            }
        }

        # Download the library
        Write-Host "Downloading $filename from $uri"
        Copy-File -SourcePath $uri -DestinationPath $localFullPath
        Write-Host ("Downloaded " + $filename + " lib archive")

        # Extract the latest library
        Write-Host "Extracting..."
        # ExtractToDirectory is at least 3x faster than Expand-Archive
        [System.IO.Compression.ZipFile]::ExtractToDirectory($localFullPath, $PSScriptRoot)
        Write-Host "Finished"

        # Clean up .zip file
        Remove-Item $localFullPath

        # Write the current version using the ETag
        Write-Version -Path $localFullPath -Version $etag
    }
}

DecompressZip -filename "Nvpipe"

