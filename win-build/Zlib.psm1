import-module ./Set-VsCmd.psm1 -Force

function New-Zlib
{
    Param(
    [string]$buildDir,
    [string]$vsVersion,
    [string]$platform,
    [string]$outputDir
    )
	pushd $buildDir
	Edit-Makefile

    $oldPath = $env:Path
	Set-VsCmd $vsVersion $platform
	
    #nmake /NOLOGO /f win32/Makefile.msc clean

	if($platform -eq "x86")
	{
		nmake /NOLOGO -f win32/Makefile.msc LOC="-DASMV -DASMINF -DNDEBUG -I." OBJA="inffas32.obj match686.obj"
	}
	elseif($platform -eq "x64")
	{
		nmake /NOLOGO -f win32/Makefile.msc AS=ml64 LOC="-DASMV -DASMINF -DNDEBUG -I." OBJA="inffasx64.obj gvmat64.obj inffas8664.obj"
	}
    $env:Path = $oldPath
	
	Move-Output $outputDir 

	popd
}

function Edit-Makefile()
{
	$fileName = "win32/Makefile.msc"

	Write-Host "$fileName.new"
	If (Test-Path "$fileName.new")
	{
		Remove-Item "$fileName.new"
	}

	Get-Content $fileName | ForEach-Object -Process {
		if($_.StartsWith("CFLAGS"))
		{
			$_ -replace '-MD','-GL -MT -Zc:wchar_t-'
		}
		elseif($_.StartsWith("LDFLAGS"))
		{
			$_ -replace '-debug','-opt:icf -dynamicbase -nxcompat -ltcg /nodefaultlib:msvcrt'
		}
		else
		{ $_; }
	} | Set-Content "$fileName.new"

	If (Test-Path "$fileName.old")
	{
		Remove-Item "$fileName.old"
	}

	Move-Item "$fileName" "$fileName.old"
	Move-Item "$fileName.new" "$fileName"
}

function Move-Output($outputDir)
{
    New-Item -ItemType Directory -Path "$outputDir" -Force
    New-Item -ItemType Directory -Path "$outputDir/include" -Force
	Copy-Item -force -Recurse zlib.h "$outputDir/include/"
	Copy-Item -force -Recurse zconf.h "$outputDir/include/"
    New-Item -ItemType Directory -Path "$outputDir/lib" -Force
	Copy-Item -force -Recurse zlib.lib "$outputDir/lib/"
	Copy-Item -force -Recurse zlib.pdb "$outputDir/lib/"
}

export-modulemember -function New-Zlib
