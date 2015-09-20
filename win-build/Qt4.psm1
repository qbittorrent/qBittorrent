import-module ./Set-VsCmd.psm1 -Force

function New-Qt4($buildDir, $vsVersion, $platform, $opensslInstalldir, $zlibInstallDir, $jomDir, $outputDir)
{
	pushd $buildDir
	
	Edit-QmakeConfg $vsVersion
    Edit-FixZlibReference

    $oldPath = $env:Path
    $env:path += ";$jomDir"
	Set-VsCmd $vsVersion $platform
    #nmake confclean
    ./configure.exe -prefix $outputDir -release -opensource -confirm-license -static -ltcg -fast -system-zlib -no-qt3support -no-opengl -no-openvg -no-dsp -no-vcproj -no-dbus -no-phonon -no-phonon-backend -no-multimedia -no-audio-backend -no-webkit -no-script -no-scripttools -no-declarative -no-declarative-debug -mp -arch windows -qt-style-windowsxp -nomake examples -nomake demos -platform win32-msvc$vsVersion -openssl-linked -largefile -I "$opensslInstalldir/include" -I "$zlibInstallDir/include" -L "$opensslInstalldir/lib" -L "$zlibInstallDir/lib"
    ./bin/qmake.exe projects.pro QT_BUILD_PARTS="libs translations"
    jom install_mkspecs
    jom
    jom install
    $env:Path = $oldPath

	popd
}


function Edit-QmakeConfg($vsVersion)
{
    $fileName = "mkspecs/win32-msvc$vsVersion/qmake.conf"

    Write-Host "$fileName.new"
    If (Test-Path "$fileName.new")
    {
	    Remove-Item "$fileName.new"
    }

    Get-Content $fileName | ForEach-Object -Process {
	    if($_.StartsWith("QMAKE_CFLAGS_RELEASE"))
	    {
		    $_ -replace '-MD','-GL -MT'
		    Write-Host "Added -GL -MT to QMAKE_CFLAGS_RELEASE in $fileName"
	    }
	    elseif($_.StartsWith("QMAKE_LFLAGS_RELEASE") -and !($_.Contains("/NODEFAULTLIB:MSVCRT")))
	    {
		    $_ -replace '/INCREMENTAL:NO','/INCREMENTAL:NO /NODEFAULTLIB:MSVCRT'
		    Write-Host "Added /NODEFAULTLIB:MSVCRT to QMAKE_LFLAGS_RELEASE in $fileName"
	    }
	    elseif($_.StartsWith("QMAKE_LFLAGS") -and !($_.Contains("/LTCG")))
	    {
		    $_ -replace '/NXCOMPAT','/NXCOMPAT /LTCG'
		    Write-Host "Added /LTCG to QMAKE_LFLAGS in $fileName"
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


function Edit-FixZlibReference()
{
    $fileNames = @()

    $fileNames += "src/3rdparty/zlib_dependency.pri"
    $fileNames += "src/tools/bootstrap/bootstrap.pri"

    foreach($fileName in $fileNames)
    {
        If (Test-Path "$fileName.new")
        {
	        Remove-Item "$fileName.new"
        }

        Get-Content $fileName | ForEach-Object -Process {
	        $_ -replace 'zdll.lib','zlib.lib'
        } | Set-Content "$fileName.new"

        If (Test-Path "$fileName.old")
        {
	        Remove-Item "$fileName.old"
        }

        Move-Item "$fileName" "$fileName.old"
        Move-Item "$fileName.new" "$fileName"
    }
}

export-modulemember -function New-Qt4