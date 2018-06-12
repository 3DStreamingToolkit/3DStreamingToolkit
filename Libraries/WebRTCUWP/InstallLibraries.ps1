. ../../Utilities/InstallLibraries.ps1

function DecompressZip {
    param( [string] $filename, [string] $path, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )

    $uri = ($blobUri + $filename + ".zip")
    $etag = Get-ETag -Uri $uri
    $localFileName = ($filename + $etag + ".zip")
    $localFullPath = ($PSScriptRoot +  $path + $localFileName)
    $libRemoved = $false
    
    if($filename -like "*libyuv*") {
        $extractDir = "\libs"
    } 

    if($filename -like "*Org.*") {
        $extractDir = "\x86"
    } 

    if((Test-Path ($PSScriptRoot +  $path)) -eq $false) {
        New-Item -Path ($PSScriptRoot +  $path) -ItemType Directory -Force 
    }

    Get-ChildItem -File -Path ($PSScriptRoot + $path) -Filter ("*" + $filename + "*") | ForEach-Object { #
        if($_.Name -notmatch (".*" + $etag + ".*")) {
                Write-Host "Removing outdated lib"
                Remove-Item * -Include $_.Name
                $libRemoved = $true
        }
    }
        
    if((Test-Path ($PSScriptRoot + $path + $extractDir)) -eq $true -and $libRemoved -eq $false) {
        return
    }

    if((Test-Path ($PSScriptRoot + $path + $extractDir)) -eq $true) {
        Write-Host "Clearing existing $path" 
        Remove-Item -Recurse -Force ($PSScriptRoot + $path + $extractDir)   
    }
    
    Write-Host "Downloading $localFileName from $uri"
    Copy-File -SourcePath $uri -DestinationPath $localFullPath    
    Write-Host ("Downloaded " + $filename + " lib archive")

    Write-Host "Extracting..."
    Expand-Archive -Path $localFullPath -DestinationPath ($PSScriptRoot + $path)
    Write-Host "Finished"
}

DecompressZip -filename "Org.WebRtc_m62_timestamp_v1" -path "\Org.WebRTC\"
DecompressZip -filename "libyuv" -path "\libyuv\"
