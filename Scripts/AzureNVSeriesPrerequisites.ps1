# Scripts to download and install NV series drivers
cd c:\
mkdir tmp
cd tmp

# Download the driver installation file
# curl https://go.microsoft.com/fwlink/?linkid=836843 -OutFile C:\tmp\385.41_grid_win10_server2016_64bit_international.exe

$url = "https://go.microsoft.com/fwlink/?linkid=836843"
$output = "C:\tmp\385.41_grid_win10_server2016_64bit_international.exe"
$start_time = Get-Date

$wc = New-Object System.Net.WebClient
$wc.DownloadFile($url, $output)

Write-Output "Time taken: $((Get-Date).Subtract($start_time).Seconds) second(s)"

# Install Tesla driver silently
.\385.41_grid_win10_server2016_64bit_international.exe /s


# TODO: Should delete installation file after installation process?
# TODO: Download + Install the MultithreadServer service