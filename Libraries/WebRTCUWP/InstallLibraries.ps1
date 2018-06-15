Add-Type -AssemblyName System.IO.Compression.FileSystem

. ../../Utilities/InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $path, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )

    # Get ETag header for current blob
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFullPath = ($PSScriptRoot + $path + $filename + ".zip")
    
    # Compare ETag against the currently installed version
    $versionMatch = Compare-Version -Path $localFullPath -Version $etag
    if (!$versionMatch) {

        $extractDir = ""

        if($filename -like "*libyuv*") {
            $extractDir = "libs"
        } 
        if($filename -like "*Org.*") {
            $extractDir = "x*"
        } 

        # Remove library archive if it already exists
        if ((Test-Path ($localFullPath))) {
            Remove-Item -Recurse -Force $localFullPath
        }

        # Download the library
        Write-Host "Downloading $filename from $uri"
        Copy-File -SourcePath $uri -DestinationPath $localFullPath
        Write-Host ("Downloaded " + $filename + " lib archive")

        # Create the lib sub-directory if it does not already exist
        if((Test-Path ($PSScriptRoot +  $path)) -eq $false) {
            New-Item -Path ($PSScriptRoot +  $path) -ItemType Directory -Force 
        }

        # Clear the files from the previous library version
        if((Test-Path ($PSScriptRoot + $path + $extractDir)) -eq $true) {
            Write-Host "Clearing existing $path" 
            Remove-Item -Recurse -Force ($PSScriptRoot + $path + $extractDir)   
        }

        # Extract the latest library
        Write-Host "Extracting..."
        # ExtractToDirectory is at least 3x faster than Expand-Archive
        [System.IO.Compression.ZipFile]::ExtractToDirectory($localFullPath, $PSScriptRoot + $path)
        Write-Host "Finished"

        # Clean up .zip file
        Remove-Item $localFullPath

        # Write the current version using the ETag
        Write-Version -Path $localFullPath -Version $etag
    }
}

DecompressZip -filename "Org.WebRtc_m62_timestamp_v1" -path "\Org.WebRTC\"
DecompressZip -filename "libyuv" -path "\libyuv\"
