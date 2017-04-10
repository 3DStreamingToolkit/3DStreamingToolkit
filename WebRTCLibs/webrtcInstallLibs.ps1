Import-Module BitsTransfer
Add-Type -AssemblyName System.IO.Compression.FileSystem

function Get-ScriptDirectory
{
  $Invocation = (Get-Variable MyInvocation -Scope 1).Value
  Split-Path $Invocation.MyCommand.Path
}

Write-Host (Test-Path ($workingDir + "\x64"))

if((Test-Path ((Get-ScriptDirectory) + "\x64.zip")) -eq $false) {
    Start-BitsTransfer -Source https://3dtoolkitstorage.blob.core.windows.net/libs/x64.zip -Destination ((Get-ScriptDirectory) + "\x64.zip")   
    Write-Host "Downloaded x64 lib archive"
} 
if((Test-Path -Path ((Get-ScriptDirectory) + "\x64")) -eq $false) {
    Write-Host "Extracting..."
    [System.IO.Compression.ZipFile]::ExtractToDirectory((Get-ScriptDirectory) + "\x64.zip", (Get-ScriptDirectory))
    Write-Host "Finished"
}

if((Test-Path ((Get-ScriptDirectory) + "\Win32.zip")) -eq $false) {
    Start-BitsTransfer -Source https://3dtoolkitstorage.blob.core.windows.net/libs/Win32.zip -Destination ((Get-ScriptDirectory) + "\Win32.zip")
    Write-Host "Downloaded Win32 lib archive"
}
if((Test-Path -Path ((Get-ScriptDirectory) + "\Win32")) -eq $false) {
    Write-Host "Extracting..."
    [System.IO.Compression.ZipFile]::ExtractToDirectory((Get-ScriptDirectory) + "\Win32.zip", (Get-ScriptDirectory))
    Write-Host "Finished"
}