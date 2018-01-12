[CmdletBinding()]
    [OutputType([PSCustomObject])]
    Param
    (
        #Root Path to Install WebRTC Source/Build
        [Parameter(Mandatory=$false,
                   Position=0)]
        $WebRTCFolder = "C:\WebRTCSource\"
    )

function Get-ScriptDirectory
{
  $Invocation = (Get-Variable MyInvocation -Scope 1).Value
  Split-Path $Invocation.MyCommand.Path
}

function Run-H264UnitTests($path)
{
  Set-Location $path
  CMD /C modules_unittests.exe --gtest_filter="H264*" --gtest_output="xml:report.xml" --run-manual
  
  [xml]$XmlDocument = Get-Content -Path report.xml
  if ($XmlDocument.testsuites.failures -gt 0) 
  {
      Write-Error -Message "Error: H264 Unit tests failed" -Category InvalidResult
      exit 1
  }

   Set-Location "../../../"
}

Import-Module BitsTransfer
Add-Type -AssemblyName System.IO.Compression.FileSystem

if((Test-Path ($WebRTCFolder)) -eq $false) {
    New-Item -Path $WebRTCFolder -ItemType Directory -Force 
}
$fullPath = (Resolve-Path -Path $WebRTCFolder).path
Set-Location $fullPath

if((Test-Path ($fullPath + "\depot_tools\")) -eq $false) {
    if(-not (Test-Path $PWD\depot_tools.zip)) {
        Start-BitsTransfer -Source https://storage.googleapis.com/chrome-infra/depot_tools.zip -Destination ((Get-ScriptDirectory) + "\depot_tools.zip")
    }
    [System.IO.Compression.ZipFile]::ExtractToDirectory(((Get-ScriptDirectory) + "\depot_tools.zip"), "$fullPath\depot_tools\")
}

if (-not($Env:Path | Select-String -SimpleMatch $fullPath\depot_tools)) { 
    $Env:Path = "$fullPath\depot_tools;" + $Env:Path
    # proper check for build agents, as they cannot write to registry.
    If (([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator"))
    {
        if(-not (Test-Path Env:SYSTEM_TEAMPROJECT)) {
            [Environment]::SetEnvironmentVariable("Path", $Env:Path, [System.EnvironmentVariableTarget]::Machine) 
        }
    }
    CMD /C "gclient"
}

$Env:DEPOT_TOOLS_WIN_TOOLCHAIN = 0
# proper check for build agents, as they cannot write to registry.
If (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(`
    [Security.Principal.WindowsBuiltInRole] "Administrator"))
{
    if(-not (Test-Path Env:SYSTEM_TEAMPROJECT)) {
        [Environment]::SetEnvironmentVariable("DEPOT_TOOLS_WIN_TOOLCHAIN", $Env:DEPOT_TOOLS_WIN_TOOLCHAIN, [System.EnvironmentVariableTarget]::Machine)
    }
}
New-Item -Path . -Name "webrtc-checkout" -ItemType Directory -Force
Set-Location "webrtc-checkout"
#if src exists, don't repull, just reset and reapply patch
if((Test-Path ("src")) -eq $false) {
    CMD /C "fetch --nohooks webrtc"
    CMD /C "gclient sync "
    Set-Location "src"
    CMD /C "gclient sync --with_branch_heads"
    CMD /C "git fetch origin"
} else {
    Set-Location "src"
    CMD /C "git reset --hard"
    CMD /C "git checkout master"
    CMD /C "git branch -D patch_branch"
}
CMD /C "git clean -f"
CMD /C "git checkout -b patch_branch refs/remotes/branch-heads/58"
CMD /C "gclient sync --jobs 16"
CMD /C ("git apply --ignore-whitespace " + $PSScriptRoot + "\nvencoder.patch")
CMD /C 'git commit -am "nvencoder patch"'
CMD /C ("git apply --ignore-whitespace " + $PSScriptRoot + "\videoframemetadata.patch")
CMD /C 'git commit -am "videoframemetadata patch"'
CMD /C ("git apply --ignore-whitespace " + $PSScriptRoot + "\nvencodernobuffer.patch")
CMD /C 'git commit -am "nvencodernobuffer patch"'

CMD /C 'gn gen citest/Win32/Release  --ide=vs --args="use_rtti=true target_cpu=\"x86\" is_debug=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_initialize_ffmpeg=true is_official_build=true"'
CMD /C 'gn gen citest/Win32/Debug    --ide=vs --args="use_rtti=true target_cpu=\"x86\" is_debug=true  rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_initialize_ffmpeg=true"'
CMD /C 'gn gen citest/x64/Release    --ide=vs --args="use_rtti=true target_cpu=\"x64\" is_debug=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_initialize_ffmpeg=true is_official_build=true"'
CMD /C 'gn gen citest/x64/Debug      --ide=vs --args="use_rtti=true target_cpu=\"x64\" is_debug=true  rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_initialize_ffmpeg=true"'

CMD /C 'ninja -C citest/Win32/Debug'
Run-H264UnitTests("citest/Win32/Debug/")

CMD /C 'ninja -C citest/x64/Debug'
Run-H264UnitTests("citest/x64/Debug")

CMD /C 'ninja -C citest/Win32/Release'
Run-H264UnitTests("citest/Win32/Release")

CMD /C 'ninja -C citest/x64/Release'
Run-H264UnitTests("citest/x64/Release")

Set-Location $PSScriptRoot
