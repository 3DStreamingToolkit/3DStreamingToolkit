Import-Module BitsTransfer
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-ScriptDirectory
{
  $Invocation = (Get-Variable MyInvocation -Scope 1).Value
  Split-Path $Invocation.MyCommand.Path
}
function DecompressZip {
    param( [string] $filename, [string] $path, [string] $blobUri = "https://3dtoolkitstorage.blob.core.windows.net/libs/" )
    $uri = ($blobUri + $filename + ".zip")
    $request = [System.Net.HttpWebRequest]::Create($uri)
    $request.Timeout = 10000
    $response = $request.GetResponse()
    $etag = $response.Headers["ETag"] 
    $request.Abort()
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
    [System.IO.Compression.ZipFile]::ExtractToDirectory($localFullPath, ($PSScriptRoot + $path))
    Write-Host "Finished"
}

function
Copy-File
{
    [CmdletBinding()]
    param(
        [string]
        $SourcePath,
        
        [string]
        $DestinationPath
    )
    
    if ($SourcePath -eq $DestinationPath)
    {
        return
    }
          
    if (Test-Path $SourcePath)
    {
        Copy-Item -Path $SourcePath -Destination $DestinationPath
    }
    elseif (($SourcePath -as [System.URI]).AbsoluteURI -ne $null)
    {
        if (Test-Nano)
        {
            $handler = New-Object System.Net.Http.HttpClientHandler
            $client = New-Object System.Net.Http.HttpClient($handler)
            $client.Timeout = New-Object System.TimeSpan(0, 30, 0)
            $cancelTokenSource = [System.Threading.CancellationTokenSource]::new() 
            $responseMsg = $client.GetAsync([System.Uri]::new($SourcePath), $cancelTokenSource.Token)
            $responseMsg.Wait()

            if (!$responseMsg.IsCanceled)
            {
                $response = $responseMsg.Result
                if ($response.IsSuccessStatusCode)
                {
                    $downloadedFileStream = [System.IO.FileStream]::new($DestinationPath, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
                    $copyStreamOp = $response.Content.CopyToAsync($downloadedFileStream)
                    $copyStreamOp.Wait()
                    $downloadedFileStream.Close()
                    if ($copyStreamOp.Exception -ne $null)
                    {
                        throw $copyStreamOp.Exception
                    }      
                }
            }  
        }
        elseif ($PSVersionTable.PSVersion.Major -ge 5)
        {
            #
            # We disable progress display because it kills performance for large downloads (at least on 64-bit PowerShell)
            #
            $ProgressPreference = 'SilentlyContinue'
            wget -Uri $SourcePath -OutFile $DestinationPath -UseBasicParsing
            $ProgressPreference = 'Continue'
        }
        else
        {
            $webClient = New-Object System.Net.WebClient
            $webClient.DownloadFile($SourcePath, $DestinationPath)
        } 
    }
    else
    {
        throw "Cannot copy from $SourcePath"
    }
}

function 
Test-Nano()
{
    $EditionId = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -Name 'EditionID').EditionId

    return (($EditionId -eq "ServerStandardNano") -or 
            ($EditionId -eq "ServerDataCenterNano") -or 
            ($EditionId -eq "NanoServer") -or 
            ($EditionId -eq "ServerTuva"))
}

DecompressZip -filename "Org.WebRtc_m58_timestamp_v2" -path "\Org.WebRTC\"
DecompressZip -filename "libyuv" -path "\libyuv\"
