# Builds a minimal static Qt (qtbase without GUI + linguist tools) with static MSVC runtime (/MT)
# for the single-file qbittorrent-nox.exe build. Run from an MSVC developer environment.
param(
    [Parameter(Mandatory = $true)][string]$Version,
    [Parameter(Mandatory = $true)][string]$Prefix,
    [Parameter(Mandatory = $true)][string]$WorkDir
)

$ErrorActionPreference = "Stop"

function Invoke-Checked {
    param([scriptblock]$Command)
    & $Command
    if ($LastExitCode -ne 0) { throw "Command failed with exit code ${LastExitCode}: $Command" }
}

New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null
Invoke-Checked { git clone --depth 1 --branch "v$Version" https://github.com/qt/qtbase.git "$WorkDir/qtbase" }
Invoke-Checked { git clone --depth 1 --branch "v$Version" https://github.com/qt/qttools.git "$WorkDir/qttools" }

# qtbase: Core, Network (Schannel TLS), Sql (bundled SQLite only), Xml
Invoke-Checked {
    cmake -G Ninja -S "$WorkDir/qtbase" -B "$WorkDir/qtbase-build" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX="$Prefix" `
        -DBUILD_SHARED_LIBS=OFF `
        -DFEATURE_static_runtime=ON `
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DQT_BUILD_BENCHMARKS=OFF `
        -DFEATURE_gui=OFF -DFEATURE_widgets=OFF -DFEATURE_dbus=OFF -DFEATURE_testlib=OFF `
        -DFEATURE_icu=OFF -DFEATURE_winsdkicu=OFF `
        -DFEATURE_openssl=OFF -DFEATURE_schannel=ON `
        -DFEATURE_sql_sqlite=ON -DFEATURE_system_sqlite=OFF `
        -DFEATURE_sql_psql=OFF -DFEATURE_sql_odbc=OFF -DFEATURE_sql_mysql=OFF `
        -DFEATURE_sql_db2=OFF -DFEATURE_sql_ibase=OFF -DFEATURE_sql_oci=OFF -DFEATURE_sql_mimer=OFF `
        -DFEATURE_system_zlib=OFF -DFEATURE_system_pcre2=OFF -DFEATURE_system_doubleconversion=OFF `
        -DFEATURE_zstd=OFF -DFEATURE_brotli=OFF
}
Invoke-Checked { cmake --build "$WorkDir/qtbase-build" }
Invoke-Checked { cmake --install "$WorkDir/qtbase-build" }

# qttools: only lrelease/lupdate/lconvert (needed by qBittorrent's CMake for translations)
Invoke-Checked {
    cmake -G Ninja -S "$WorkDir/qttools" -B "$WorkDir/qttools-build" `
        -DCMAKE_TOOLCHAIN_FILE="$Prefix/lib/cmake/Qt6/qt.toolchain.cmake" `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX="$Prefix" `
        -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF `
        -DFEATURE_linguist=ON -DFEATURE_assistant=OFF -DFEATURE_designer=OFF -DFEATURE_pixeltool=OFF `
        -DFEATURE_qtattributionsscanner=OFF -DFEATURE_qtplugininfo=OFF -DFEATURE_qtdiag=OFF `
        -DFEATURE_kmap2qmap=OFF -DFEATURE_qev=OFF -DFEATURE_qdbus=OFF -DFEATURE_clang=OFF -DFEATURE_qdoc=OFF
}
Invoke-Checked { cmake --build "$WorkDir/qttools-build" }
Invoke-Checked { cmake --install "$WorkDir/qttools-build" }
