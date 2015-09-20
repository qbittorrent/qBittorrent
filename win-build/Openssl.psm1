import-module ./Set-VsCmd.psm1 -Force

function New-Openssl
{
    Param(
    [string]$buildDir,
    [string]$vsVersion,
    [string]$platform,
    [string]$zlibDir,
    [string]$nasmDir,
    [string]$outputDir
    )
    $oldPath = $env:Path
    Set-VsCmd $vsVersion $platform
    pushd $buildDir

    $env:path += ";$nasmDir"

	if($platform -eq "x86")
	{
		perl Configure VC-WIN32 zlib --with-zlib-lib=zlib.lib -I"$zlibDir"/include -L"$zlibDir"/lib
		ms/do_nasm
	}
	else
	{
		perl Configure VC-WIN64A zlib --with-zlib-lib=zlib.lib threads -I"$zlibDir"/include -L"$zlibDir"/lib
		ms/do_win64a.bat
	}

    Edit-NtMak

    #nmake -f ms/nt.mak clean
    nmake /NOLOGO -f ms/nt.mak all

    $env:Path = $oldPath

    New-Item -ItemType Directory -Path "$outputDir/include" -Force
    New-Item -ItemType Directory -Path "$outputDir/lib" -Force

    Copy-Item -force -Recurse "out32/*.lib" "$outputDir/lib/"
	Copy-Item -force -Recurse "inc32/*" "$outputDir/include/"

    popd
}


function Edit-NtMak()
{
    $fileName = "ms/nt.mak"

    Write-Host "$fileName.new"
    If (Test-Path "$fileName.new")
    {
	    Remove-Item "$fileName.new"
    }

    Get-Content $fileName | ForEach-Object -Process {
	    if($_.StartsWith("all:") -and $_.Contains("headers lib exe"))
	    {
            $_ -replace 'headers lib exe','headers lib'
		    Write-Host $_
	    }
	    elseif($_.StartsWith("CFLAG=") -and !($_.Contains("/Zc:wchar_t- /GL /Zi")))
	    {
		    $_ = $_.TrimEnd() + " /Zc:wchar_t- /GL /Zi"
		    $_
		    Write-Host $_
	    }
	    elseif($_.StartsWith("LFLAGS="))
	    {
		    $_ -replace '/debug','/incremental:no /opt:icf /dynamicbase /ltcg /nodefaultlib:msvcrt'
		    Write-Host $_
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

export-modulemember -function New-Openssl
