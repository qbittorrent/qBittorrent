Import-Module ./Zlib.psm1 -Force
Import-Module ./Openssl.psm1 -Force
Import-Module ./Libtorrent.psm1 -Force
Import-Module ./Qt4.psm1 -Force
Import-Module ./Boost.psm1 -Force

function New-Prerequisite($prerequisitePath, $libsVersions, $toolsVersions, $vsVersion, $platform, $libsRoot)
{
    $libsRoot += "/msvc$vsVersion-$platform"
    	
	if (!(Test-Path $prerequisitePath))
	{
		New-Item -ItemType Directory -Path $prerequisitePath | Out-Null
	}

    $7zaPackage =  New-7za-Package $toolsVersions["7za"]
    Download-Package $prerequisitePath $7zaPackage.ArchiveFile $7zaPackage.DownloadUrl
    Setup7za $prerequisitePath $7zaPackage.ArchiveFile $7zaPackage.ArchiveRootDirectoryName $7zaPackage.Name
    
	$packages = @()

	$packages += New-Nasm-Package $toolsVersions["nasm"]
    $packages += New-Jom-Package $toolsVersions["jom"]
	$packages += New-Nsis-Package $toolsVersions["nsis"]


	foreach ($package in $packages)
	{
		 Download-Package $prerequisitePath $package.ArchiveFile $package.DownloadUrl
	}
	
	foreach ($package in $packages)
	{
		 Unpack-Package $prerequisitePath $package.ArchiveFile $package.ArchiveRootDirectoryName $package.Name
	}

    # Key representing a specific libs and build configuration
    $libsKey = ""
    foreach($libVersion in $libsVersions.Keys)
    {
        $libsKey += $libVersion
        $libsKey += $libsVersions[$libVersion]
    }
    $libsKey += "msvc$vsVersion-$platform"

    $buildedDir = @{zlib = "$libsRoot/zlib-$($libsVersions["zlib"])-msvc$vsVersion-$platform";
                   boost = "$libsRoot/boost-$($libsVersions["boost"])-msvc$vsVersion-$platform";
                   openssl = "$libsRoot/openssl-$($libsVersions["openssl"])-msvc$vsVersion-$platform";
                   qt = "$libsRoot/qt-$($libsVersions["qt"])-msvc$vsVersion-$platform";
                   libtorrent =  "$libsRoot/libtorrent-$($libsVersions["libtorrent"])-msvc$vsVersion-$platform" }


    if(!(artifactAvailable($libsKey)))
    {
        # Download sources

        $packages = @()
    
	    $packages += New-Zlib-Package $libsVersions["zlib"]
	    $packages += New-Openssl-Package $libsVersions["openssl"]
	    $packages += New-Libtorrent-Package $libsVersions["libtorrent"]
	    $packages += New-Boost-Package $libsVersions["boost"]
	    $packages += New-Qt4-Package $libsVersions["qt"]

        foreach ($package in $packages)
	    {
            Download-Package $prerequisitePath $package.ArchiveFile $package.DownloadUrl
	    }
	
	    foreach ($package in $packages)
	    {
            Unpack-Package $prerequisitePath $package.ArchiveFile $package.ArchiveRootDirectoryName $package.Name
	    }

        # Build everything


        New-Zlib -buildDir "$prerequisitePath/zlib" -vsVersion $vsVersion -platform $platform -outputDir $buildedDir.zlib
        New-Openssl -buildDir "$prerequisitePath/openssl" -vsVersion $vsVersion -platform $platform -zlibDir $buildedDir.zlib -nasmDir "$prerequisitePath/nasm" -outputDir $buildedDir.openssl
        New-Boost -buildDir "$prerequisitePath/boost" -vsVersion $vsVersion -platform $platform -outputDir $buildedDir.boost
        New-Libtorrent -buildDir "$prerequisitePath/libtorrent" -vsVersion $vsVersion -platform $platform -boostDir "$prerequisitePath/boost" -opensslInstallDir $buildedDir.openssl -outputDir $buildedDir.libtorrent
        New-Qt4 "$prerequisitePath/qt4" $vsVersion $platform $buildedDir.openssl $buildedDir.zlib "$prerequisitePath/jom" $buildedDir.qt

        # Compress

        pushd $libsRoot

        7za a "$libsKey.7z" "*"

        popd

        # Push artifact to Appveyor

        Push-AppveyorArtifact "$libsRoot/$libsKey.7z" -DeploymentName $libsKey
    }
    else
    {
        # Download artifact from Appveyor and extract
    }
    $buildedDir
}

function artifactAvailable($key)
{
    $false
}

function New-7za-Package($version)
{
	return New-Package "7za" $version "http://www.7-zip.org/a/7za$($version.replace('.', '')).zip" $null
}

function New-Boost-Package($version)
{
	return New-Package "boost" $version "http://freefr.dl.sourceforge.net/project/boost/boost/$version/boost_$($version.replace('.', '_')).zip" "boost_$($version.replace('.', '_'))"
}

function New-Zlib-Package($version)
{
	return New-Package "zlib" $version "https://codeload.github.com/madler/zlib/zip/v$version" "zlib-$version"
}

function New-Openssl-Package($version)
{
	return New-Package "openssl" $version "https://codeload.github.com/openssl/openssl/zip/OpenSSL_$($version.replace('.', '_'))" "openssl-OpenSSL_$($version.replace('.', '_'))"
}

function New-Libtorrent-Package($version)
{
	return New-Package "libtorrent"	$version "https://codeload.github.com/arvidn/libtorrent/zip/libtorrent-$($version.replace('.', '_'))" "libtorrent-libtorrent-$($version.replace('.', '_'))"
}

function New-Nasm-Package($version)
{
	return New-Package "nasm" $version "http://www.nasm.us/pub/nasm/releasebuilds/$version/win32/nasm-$version-win32.zip" "nasm-$version"
}

function New-Qt4-Package($version)
{
	return New-Package "qt4" $version "http://download.qt.io/official_releases/qt/$($version.Substring(0,3))/$version/qt-everywhere-opensource-src-$version.zip" "qt-everywhere-opensource-src-$version"
}

function New-Jom-Package($version)
{
	return New-Package "jom" $version "http://download.qt.io/official_releases/jom/jom_$($version.replace('.', '_')).zip" $null
}

function New-Nsis-Package($version)
{
	return New-Package "nsis" $version "http://heanet.dl.sourceforge.net/project/nsis/NSIS 3 Pre-release/$version/nsis-$version.zip" "nsis-$version"
}


function New-Package
{
  param ($name, $version, $downloadUrl, $archiveRootDirectoryName)

  $package = new-object PSObject

  $package | add-member -type NoteProperty -Name Name -Value $name
  $package | add-member -type NoteProperty -Name Version -Value $version
  $package | add-member -type NoteProperty -Name DownloadUrl -Value $downloadUrl
  $package | add-member -type NoteProperty -Name ArchiveRootDirectoryName -Value $archiveRootDirectoryName
  $package | add-member -type NoteProperty -Name ArchiveFile -Value "$name-$version.zip"

  return $package
}

function Download-File {
    param (
        [string]$url,
        [string]$target
    )
	Write-Host "Downloading $url"
    $webClient = new-object System.Net.WebClient
    $webClient.DownloadFile($url, $target)
}

function Setup7za($packagesPath, $archiveFile, $archiveRootDirectoryName, $outputDirectoryName)
{
    $7zaDir = (Join-Path $packagesPath $outputDirectoryName)
    if (!(Test-Path $7zaDir))
    {
        [System.Reflection.Assembly]::LoadWithPartialName('System.IO.Compression.FileSystem') | Out-Null
        [System.IO.Compression.ZipFile]::ExtractToDirectory((Join-Path $packagesPath $archiveFile), $7zaDir)
    }
    set-alias 7za "$7zaDir/7za.exe" -scope Global
}

function Extract-File {
    param (
        [string]$file,
        [string]$target
    )
        7za x "$file" -y -o"$target"

}

function Download-Package($packagesPath, $archiveFile, $downloadUrl)
{
	if (!(Test-Path (Join-Path $packagesPath $archiveFile))) {
		Write-Host "Downloading $archiveFile"
		Download-File $downloadUrl (Join-Path $packagesPath $archiveFile)
	}
} 
 
function Unpack-Package($packagesPath, $archiveFile, $archiveRootDirectoryName, $outputDirectoryName)
{
	if (!(Test-Path  (Join-Path $packagesPath $outputDirectoryName))) {
		Write-Host "Unpacking $archiveFile"

        if(!$archiveRootDirectoryName)
        {
            Extract-File (Join-Path $packagesPath $archiveFile) (Join-Path $packagesPath $outputDirectoryName)
        }
        else
        {
        	Extract-File (Join-Path $packagesPath $archiveFile) $packagesPath
		    Rename-Item (Join-Path $packagesPath $archiveRootDirectoryName) $outputDirectoryName
        }
	}
}

function Hash($textToHash)
{
    $hasher = new-object System.Security.Cryptography.SHA256Managed
    $toHash = [System.Text.Encoding]::UTF8.GetBytes($textToHash)
    $hashByteArray = $hasher.ComputeHash($toHash)
    foreach($byte in $hashByteArray)
    {
         $res += $byte.ToString()
    }
    return $res;
}

export-modulemember -function New-Prerequisite