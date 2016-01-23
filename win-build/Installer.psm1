function New-Installer($buildDir, $vsVersion, $platform,  $nsisDir, $installer)
{
	pushd $buildDir

    New-Item -ItemType Directory -Path "$installer" -Force	

    Copy-Item -force -Recurse "$buildDir/src/release/qbittorrent.exe" "$installer/"
    Copy-Item -force -Recurse "$buildDir/src/release/qbittorrent.pdb" "$installer/"
    Copy-Item -force -Recurse "$buildDir/COPYING" "$installer/license.txt"
    Copy-Item -force -Recurse "$buildDir/dist/windows/*.ns*" "$installer/"
    Copy-Item -force -Recurse "$buildDir/dist/windows/qt.conf*" "$installer/"
    
    Copy-Item -force -Recurse "$buildDir/dist/qt-translations*" "$installer/translations"
    Copy-Item -force -Recurse "$buildDir/dist/windows/installer-translations" "$installer/"

    # Extract extra plugins in installer folder
    $extraPluginsPath = "$buildDir/dist/windows/nsis plugins"
    $extraPluginsExtractedPath = "$installer/nsis-plugins"

    New-Item -ItemType Directory -Path "$extraPluginsExtractedPath" -Force	

    Extract-File "$extraPluginsPath/FindProcDLL Unicode bin.zip" "$extraPluginsExtractedPath"
    Extract-File "$extraPluginsPath/nsisFirewall.zip" "$extraPluginsExtractedPath"
    Extract-File "$extraPluginsPath/UAC Unicode.zip" "$extraPluginsExtractedPath"

    # Copy extra plugins in to NSIS installation
    Copy-Item -force -Recurse "$extraPluginsExtractedPath/FindProcDLL.dll" "$nsisDir/Plugins/x86-unicode/"
    Copy-Item -force -Recurse "$extraPluginsExtractedPath/bin/nsisFirewallW.dll" "$nsisDir/Plugins/x86-unicode/"
    Copy-Item -force -Recurse "$extraPluginsExtractedPath/UAC.dll" "$nsisDir/Plugins/x86-unicode/"

    popd

    pushd $installer

    & $nsisDir/makensis "qbittorrent.nsi"

    Copy-Item -force -Recurse "*setup.exe" "$buildDir/src/release/"

	popd
}

function Extract-File {
    param (
        [string]$file,
        [string]$target
    )

    [System.Reflection.Assembly]::LoadWithPartialName('System.IO.Compression.FileSystem') | Out-Null
    [System.IO.Compression.ZipFile]::ExtractToDirectory($file, $target)
}

export-modulemember -function New-Installer