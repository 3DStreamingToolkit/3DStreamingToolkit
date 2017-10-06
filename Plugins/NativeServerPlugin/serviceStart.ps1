$serviceName = "RenderService"
$configPath = ($PSScriptRoot + "\serverConfig.json")
foreach ($line in Get-Content $configPath) {
	$line = $line.Trim()
    if ($line.StartsWith("`"name`":")) {
		$start = $line.IndexOf(":") + 3
		$length = $line.length - 2 - $start
        $serviceName = $line.Substring($start, $length)
    }
}

Start-Service -Name $serviceName
