import-module ./Set-VsCmd.psm1 -Force

function New-Boost
{
    Param(
    [string]$buildDir,
    [string]$vsVersion,
    [string]$platform,
    [string]$outputDir
    )

    $oldPath = $env:Path
    Set-VsCmd $vsVersion $platform
    pushd $buildDir

    $vsNumber = Get-VsNumberFromYear $vsVersion
    
    ./bootstrap.bat vc$vsNumber
    ./b2 -q install --prefix=$outputDir --layout=system --with-system --toolset=msvc-$vsNumber.0 variant=release link=static runtime-link=static debug-symbols=off warnings=off warnings-as-errors=off
    
    $env:Path = $oldPath
    popd
}

export-modulemember -function New-Boost
