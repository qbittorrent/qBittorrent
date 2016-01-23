import-module ./Set-VsCmd.psm1 -Force

function New-Libtorrent
{
    Param(
    [string]$buildDir,
    [string]$vsVersion,
    [string]$platform,
    [string]$boostDir,
    [string]$opensslInstallDir,
    [string]$outputDir
    )
    
   
    $oldPath = $env:Path
    Set-VsCmd $vsVersion $platform
    $vsNumber = Get-VsNumberFromYear $vsVersion
    $env:path += ";$boostDir"

    # We assume that Boost.Build is already built during boost build
    #pushd $boostDir
    #bootstrap.bat vc$vsNumber
    #popd

    pushd $buildDir
    if($platform -eq "x86")
	{
        b2 -q --prefix="$outputDir" --toolset=msvc-$vsNumber.0 variant=release link=static runtime-link=static encryption=openssl logging=none geoip=static dht=on boost=source character-set=unicode boost-link=static -sBOOST_ROOT="$boostDir" include="$opensslInstallDir/include" library-path="$opensslInstallDir/lib" install
	}
	elseif($platform -eq "x64")
	{
        b2 -q --prefix="$outputDir" --toolset=msvc-$vsNumber.0 variant=release link=static runtime-link=static encryption=openssl logging=none geoip=static dht=on boost=source character-set=unicode boost-link=static -sBOOST_ROOT="$boostDir" architecture=x86 address-model=64 install
	}
    $env:Path = $oldPath

    # Remove libboost that has been copied to libtorrent install folder
    Remove-Item -Path "$outputDir/lib/libboost*.lib"
  
    popd
}

function Move-Output($buildDir, $outputDir)
{
	Copy-Item -force -Recurse "$buildDir/stage/lib/*.lib" "$outputDir/lib/"
}

export-modulemember -function New-Libtorrent