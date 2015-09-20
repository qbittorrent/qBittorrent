Import-Module ./Prerequisite.psm1 -Force
Import-Module ./QBittorrent.psm1 -Force
Import-Module ./Installer.psm1 -Force

$vsVersion = "2013"
$platform = "x86"
$libsRoot =  "C:/qbtl"
$installer =  "C:/qbittorrent-installer-$vsVersion-$platform"
$prerequisitePath = "C:/qbp"
$buildDir = (get-item $PSScriptRoot ).parent.Fullname

$libsVersions = @{"zlib" = "1.2.8"; "boost" = "1.55.0"; openssl = "1.0.1p"; qt = "4.8.7"; libtorrent =  "1.0.6" }

$toolsVersions = @{"7za" = "9.20"; nsis = "3.0a0"; jom = "1.0.16"; nasm = "2.11.08" }

$buildedDir = New-Prerequisite $prerequisitePath $libsVersions $toolsVersions $vsVersion $platform $libsRoot
New-QBittorrent $buildDir $vsVersion $platform $buildedDir.qt $buildedDir.boost $buildedDir.libtorrent $buildedDir.zlib $buildedDir.openssl "$prerequisitePath/jom" 
New-Installer $buildDir $vsVersion $platform  "$prerequisitePath/nsis" $installer




