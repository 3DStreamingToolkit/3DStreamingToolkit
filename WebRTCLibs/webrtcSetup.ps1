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
    $fullPath = (Resolve-Path -Path $WebRTCFolder).path
}

if(-not (Test-Path $PWD\depot_tools.zip)) {
    Start-BitsTransfer -Source https://storage.googleapis.com/chrome-infra/depot_tools.zip -Destination ((Get-ScriptDirectory) + "\depot_tools.zip")
}
[System.IO.Compression.ZipFile]::ExtractToDirectory(((Get-ScriptDirectory) + "\depot_tools.zip"), "$fullPath\depot_tools\")

IF (-not($Env:Path | Select-String -SimpleMatch $fullPath\depot_tools)) { 
    $Env:Path = "$fullPath\depot_tools;" + $Env:Path
    [Environment]::SetEnvironmentVariable("Path", $Env:Path, [System.EnvironmentVariableTarget]::Machine) 
}
Set-Location $fullPath
CMD /C "gclient"
$Env:DEPOT_TOOLS_WIN_TOOLCHAIN = 0
[Environment]::SetEnvironmentVariable("DEPOT_TOOLS_WIN_TOOLCHAIN", $Env:DEPOT_TOOLS_WIN_TOOLCHAIN, [System.EnvironmentVariableTarget]::Machine)
New-Item -Path . -Name "webrtc-checkout" -ItemType Directory -Force
Set-Location "webrtc-checkout"
CMD /C fetch --nohooks webrtc
CMD /C gclient sync
Set-Location "src"
git checkout master

CMD /C 'gn gen out/VsBuild86Release --ide=vs --args="use_rtti=true target_cpu=\"x86\" is_debug=true rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'gn gen out/VsBuild64Release --ide=vs --args="use_rtti=true is_debug=false rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'gn gen out/VsBuild64 --ide=vs --args="use_rtti=true rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'gn gen out/VsBuild86 --ide=vs --args="use_rtti=true target_cpu=\"x86\" rtc_use_h264=true ffmpeg_branding=\"Chrome\" use_openh264=true rtc_include_tests=false libyuv_include_tests=false build_libsrtp_tests=false rtc_initialize_ffmpeg=true"'
CMD /C 'ninja -C out/VsBuild86'
CMD /C 'ninja -C out/VsBuild64'
CMD /C 'ninja -C out/VsBuild86Release'
CMD /C 'ninja -C out/VsBuild64Release'
