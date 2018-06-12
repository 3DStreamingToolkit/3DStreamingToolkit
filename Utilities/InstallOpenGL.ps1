. .\InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFileName = ($filename + $etag + ".zip")
    $localFullPath = ($PSScriptRoot + "\..\Libraries\" + $localFileName)
    
    Get-ChildItem -File -Path $PSScriptRoot -Filter ("*" + $filename + "*") | ForEach-Object { #
        if($_.Name -notmatch (".*" + $etag + ".*")) {
                Write-Host "Removing outdated lib"
                Remove-Item * -Include $_.Name
        }
    }

    if((Test-Path ($PSScriptRoot + "\..\Libraries\Freeglut")) -eq $false) {
        Write-Host ("Downloading " + $filename + " lib archive")
        if((Test-Path ($localFullPath)) -eq $false) {
               Copy-File -SourcePath $uri -DestinationPath $localFullPath    
               Write-Host ("Downloaded " + $filename + " lib archive")
        }
        Write-Host "Extracting..."
        Expand-Archive -Path $localFullPath -DestinationPath ($PSScriptRoot + "\..\Libraries\")
        Write-Host "Finished"
    }
}

DecompressZip -filename "libOpenGL"
