import-module ./Set-VsCmd.psm1 -Force

function New-QBittorrent($buildDir, $vsVersion, $platform, $qtDir, $boostDir, $libtorrentDir, $zlibDir, $opensslDir, $jomDir)
{
	pushd $buildDir

    EditWinconfPri $boostDir $libtorrentDir $zlibDir $opensslDir

    $oldPath = $env:Path
	Set-VsCmd $vsVersion $platform

    $env:path += ";$qtDir/bin;$jomDir"

	qmake qbittorrent.pro -r -spec win32-msvc$vsVersion
    jom clean
    jom
    $env:Path = $oldPath
	popd

}

function EditWinconfPri($boostDir, $libtorrentDir, $zlibDir, $opensslDir)
{
	$fileName = "winconf.pri"

	Write-Host "$fileName.new"
	If (Test-Path "$fileName.new")
	{
		Remove-Item "$fileName.new"
	}

	Get-Content $fileName | ForEach-Object -Process { 
	$_ -replace '<boostDir>', $boostDir -replace '<libtorrentDir>', "$libtorrentDir" -replace '<zlibDir>', "$zlibDir" -replace '<opensslDir>', "$opensslDir"
	} | Set-Content "$fileName.new"

	If (Test-Path "$fileName.old")
	{
		Remove-Item "$fileName.old"
	}

	Move-Item "$fileName" "$fileName.old"
	Move-Item "$fileName.new" "$fileName"
}

export-modulemember -function New-QBittorrent