Add-Type -AssemblyName System.IO.Compression.FileSystem

. .\InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $libraryPath = $PSScriptRoot + "\..\Libraries\"
    $localFullPath = ($libraryPath + $filename + ".zip")

    # Compare ETag against the currently installed version
    $versionMatch = Compare-Version -Path $localFullPath -Version $etag
    if (!$versionMatch) {

        # Remove library archive if it already exists
        if ((Test-Path ($localFullPath))) {
            Remove-Item -Recurse -Force $localFullPath
        }

        # Download the library
        Write-Host "Downloading $filename from $uri"
        Copy-File -SourcePath $uri -DestinationPath $localFullPath
        Write-Host ("Downloaded " + $filename + " lib archive")

        # Clear the files from the previous library version
        $freeglutPath = $libraryPath + "Freeglut"
        $glewPath = $libraryPath + "Glew"
        $glextPath = $libraryPath + "glext"
        @( $freeglutPath, $glewPath, $glextPath ) | ForEach-Object {
            if((Test-Path ($_)) -eq $true) {
                Write-Host "Clearing existing $_" 
                Remove-Item -Recurse -Force ($_)
            }
        }

        # Extract the latest library
        Write-Host "Extracting..."
        # ExtractToDirectory is at least 3x faster than Expand-Archive
        [System.IO.Compression.ZipFile]::ExtractToDirectory($localFullPath, $libraryPath)
        Write-Host "Finished"

        # Clean up .zip file
        Remove-Item $localFullPath

        # Write the current version using the ETag
        Write-Version -Path $localFullPath -Version $etag
    }
}

DecompressZip -filename "libOpenGL"
