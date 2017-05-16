Import-Module BitsTransfer
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-ScriptDirectory
{
  $Invocation = (Get-Variable MyInvocation -Scope 1).Value
  Split-Path $Invocation.MyCommand.Path
}

function DecompressZip {
    param( [string] $filename, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    
    $uri = ($blobUri + $filename + ".zip")
    $request = [System.Net.HttpWebRequest]::Create($uri)
    $request.Timeout = 10000
    $response = $request.GetResponse()
    $etag = $response.Headers["ETag"] 
    $request.Abort()
    $localFileName = ($filename + $etag + ".zip")
    $localFullPath = ($PSScriptRoot + "\" + $localFileName)
    
    Get-ChildItem -File -Path $PSScriptRoot -Filter ("*" + $filename + "*") | ForEach-Object { #
        if($_.Name -notmatch (".*" + $etag + ".*")) {
                Write-Host "Removing outdated lib"
                Remove-Item * -Include $_.Name
        }
    }



    if((Test-Path ($localFullPath)) -eq $false) {
        Start-BitsTransfer -Source $uri -Destination $localFullPath    
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
        
        if((Test-Path ($PSScriptRoot + $extractDir)) -eq $true) {
            Write-Host "Clearing existing $extractDir" 
            Remove-Item -Recurse -Force ($PSScriptRoot + $extractDir)
        }

        Write-Host "Extracting..."
        [System.IO.Compression.ZipFile]::ExtractToDirectory($localFullPath, $PSScriptRoot)
        Write-Host "Finished"
    }
}


DecompressZip -filename "m58patch_headers"
DecompressZip -filename "m58patch_x64"
DecompressZip -filename "m58patch_Win32"
