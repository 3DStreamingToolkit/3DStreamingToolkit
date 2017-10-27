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

CMD /C 'gn gen out/Win32/Release  --ide=vs --args="target_cpu=\"x86\" is_debug=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true is_official_build=true"'
CMD /C 'gn gen out/Win32/Debug    --ide=vs --args="target_cpu=\"x86\" is_debug=true  rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'gn gen out/x64/Release    --ide=vs --args="target_cpu=\"x64\" is_debug=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true is_official_build=true"'
CMD /C 'gn gen out/x64/Debug      --ide=vs --args="target_cpu=\"x64\" is_debug=true  rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'ninja -C out/Win32/Debug'
CMD /C 'ninja -C out/x64/Debug'
CMD /C 'ninja -C out/Win32/Release'
CMD /C 'ninja -C out/x64/Release'

# Testing Builds for WINRT Libs.  Currently broken, do not enable
# CMD /C 'gn gen out/Win32/Release  --ide=vs --args="use_rtti=true target_cpu=\"x86\" is_debug=false target_os=\"winrt_10\" current_os=\"winrt_10\""'
# CMD /C 'gn gen out/Win32/Debug    --ide=vs --args="use_rtti=true target_cpu=\"x86\" is_debug=true  target_os=\"winrt_10\" current_os=\"winrt_10\""'
# CMD /C 'gn gen out/x64/Release    --ide=vs --args="use_rtti=true target_cpu=\"x64\" is_debug=false target_os=\"winrt_10\" current_os=\"winrt_10\""'
# CMD /C 'gn gen out/x64/Debug      --ide=vs --args="use_rtti=true target_cpu=\"x64\" is_debug=true  target_os=\"winrt_10\" current_os=\"winrt_10\""'


$outPath = New-Item -Path ..\ -Name "dist" -ItemType directory -Force
Set-Location "third_party"

Get-ChildItem -Recurse -Filter "*.h" | ForEach-Object {
    $touchItem = New-Item (Join-Path -Path ($outPath.FullName + "\headers\third_party\") -ChildPath (Resolve-Path -Path $_.FullName -Relative))  -ItemType file -Force
    Copy-Item $_.FullName -Destination $touchItem.FullName -Force
}
Set-Location "../webrtc"

Get-ChildItem -Recurse -Filter "*.h" | ForEach-Object {
    $touchItem = New-Item (Join-Path -Path ($outPath.FullName + "\headers\webrtc\") -ChildPath (Resolve-Path -Path $_.FullName -Relative))  -ItemType file -Force
    Copy-Item $_.FullName -Destination $touchItem.FullName -Force
}
Set-Location "..\out\"
Get-ChildItem -Directory | ForEach-Object {
    Get-ChildItem -Path $_.FullName -Directory | ForEach-Object {
         $libPath = New-Item -Path ($outPath.FullName + "\" + (Resolve-Path -Path $_.FullName -Relative)) -ItemType Directory -Force

        Get-ChildItem -Path $_.FullName -Recurse -Filter "*.pdb" | Where-Object {
            $_.Name -notmatch ".*exe.*" -and $_.Name -notmatch ".*dll.*" } | ForEach-Object {
            $touchItem = New-Item -Path ($libPath.FullName + "\lib") -Name $_.Name  -ItemType file -Force
            Copy-Item $_.FullName -Destination $touchItem.FullName -Force
        }
        Get-ChildItem -Path $_.FullName -Recurse -Filter "*.lib" | Where-Object {
            $_.Name -match "common_video.lib|ffmpeg.dll.lib|webrtc.lib|boringssl_asm.lib|boringssl.dll.lib|field_trial_default.lib|metrics_default.lib|protobuf_full.lib|libyuv.lib" } | ForEach-Object {
            $touchItem = New-Item -Path ($libPath.FullName + "\lib") -Name $_.Name  -ItemType file -Force
            Copy-Item $_.FullName -Destination $touchItem.FullName -Force
        }
        Get-ChildItem -Path $_.FullName -Recurse -File | Where-Object {
            $_.Name -match "\.exe|\.exe\.pdb" } | ForEach-Object {
            $touchItem = New-Item -Path ($libPath.FullName + "\exe") -Name $_.Name -ItemType file -Force
            Copy-Item $_.FullName -Destination $touchItem.FullName -Force
        }
        Get-ChildItem -Path $_.FullName -Recurse -Filter "*.dll" | Where-Object {
            $_.Name -notmatch "api-ms-win.*|API-MS-WIN.*|vc.*"} | ForEach-Object {
            $touchItem = New-Item -Path ($libPath.FullName + "\dll") -Name $_.Name  -ItemType file -Force
            Copy-Item $_.FullName -Destination $touchItem.FullName -Force
        }
    }
}
Set-Location $PSScriptRoot
