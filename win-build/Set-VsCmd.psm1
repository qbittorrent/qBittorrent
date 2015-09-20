function Set-VsCmd
{
    param(
        [parameter(Mandatory, HelpMessage="Enter VS version as 2010, 2012, or 2013")]
        [ValidateSet(2010,2012,2013,2015)]
        [int]$version,
		[parameter(Mandatory, HelpMessage="Enter platform as x86, x64")]
		[ValidateSet("x86","x64")]
		[string]$platform
    )
	
	if($platform -eq "x64")
	{
		$platformStr = "amd64"
	}
	elseif($platform -eq "x86")
	{
		$platformStr = "x86"
	}
	
    $VS_VERSION = @{ 2010 = "10"; 2012 = "11"; 2013 = "12"; 2015 = "14" }
    $vsCommonToolsDirectoryPath = dir "env:VS$($VS_VERSION[$version])0COMNTOOLS" | Select-Object -ExpandProperty Value
	
	$targetDir = (Resolve-Path (Join-Path $vsCommonToolsDirectoryPath '../../VC')).Path
	
    if (!(Test-Path (Join-Path $targetDir "vcvarsall.bat"))) {
        "Error: Visual Studio $version not installed"
        return
    }
    pushd $targetDir
    cmd /c "vcvarsall.bat $platformStr &set" |
    foreach {
      if ($_ -match "(.*?)=(.*)") {
        Set-Item -force -path "ENV:\$($matches[1])" -value "$($matches[2])"
      }
    }
    popd
    write-host "`nVisual Studio $version Command Prompt variables set." -ForegroundColor Yellow
}

function Get-VsNumberFromYear
{
    param(
        [parameter(Mandatory, HelpMessage="Enter VS year")]
        [ValidateSet(2010,2012,2013,2015)]
        [int]$year
    )
	$VS_VERSION = @{ 2010 = "10"; 2012 = "11"; 2013 = "12"; 2015 = "14" }
	$VS_VERSION.$year
}

export-modulemember -function Set-VsCmd
export-modulemember -function Get-VsNumberFromYear