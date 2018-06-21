# run all *.Test.exe binaries, passing gtest_output flags to redirect output to xml
# then exit with any failing exit code (if any) or 0
#
# note: we capture stdout and stderr because vsts ci builds treat stderr as failure
# but gtest does not. so we must proxy the data.
$errFail = 0
Get-ChildItem -Path "$PSScriptRoot\.." -Filter *.Tests.exe -Recurse | %{
  $pinfo = New-Object System.Diagnostics.ProcessStartInfo
  $pinfo.FileName = $_.FullName
  $pinfo.WorkingDirectory = $_.DirectoryName
  $pinfo.RedirectStandardError = $true
  $pinfo.RedirectStandardOutput = $true
  $pinfo.UseShellExecute = $false
  $pinfo.Arguments = "--gtest_output=xml:$($_.FullName)-Test.xml"
  $p = New-Object System.Diagnostics.Process
  $p.StartInfo = $pinfo
  $p.Start() | Out-Null
  $stdout = $p.StandardOutput.ReadToEnd()
  $stderr = $p.StandardError.ReadToEnd()
  $p.WaitForExit()
  Write-Host $stdout
  Write-Host $stderr
  Write-Host "Exit Code: $($p.ExitCode)"
  if ($p.ExitCode -ne 0) {
    $errFail = $LASTEXITCODE
  }
  $p.Dispose()
}
Write-Host "Exiting test run with code: $errFail"
exit $errFail