#!/bin/bash -e
# This scrip is for building AppImage
# Please run this scrip in docker image: ubuntu:16.04
# E.g: docker run --rm -v `git rev-parse --show-toplevel`:/build ubuntu:16.04 /build/.github/workflows/build_appimage.sh
# Artifacts will copy to the same directory.

# Ubuntu mirror for local building
# source /etc/os-release
# cat >/etc/apt/sources.list <<EOF
# deb http://opentuna.cn/ubuntu/ ${UBUNTU_CODENAME} main restricted universe multiverse
# deb http://opentuna.cn/ubuntu/ ${UBUNTU_CODENAME}-updates main restricted universe multiverse
# deb http://opentuna.cn/ubuntu/ ${UBUNTU_CODENAME}-backports main restricted universe multiverse
# deb http://opentuna.cn/ubuntu/ ${UBUNTU_CODENAME}-security main restricted universe multiverse
# EOF

apt update
apt install -y software-properties-common
apt-add-repository -y ppa:savoury1/backports
apt-add-repository -y ppa:savoury1/gcc-defaults-9
apt update
apt install -y --no-install-suggests --no-install-recommends \
  curl \
  gcc \
  g++ \
  make \
  autoconf \
  automake \
  pkg-config \
  file \
  zlib1g-dev \
  libssl-dev \
  libtool \
  python3-semantic-version \
  python3-lxml \
  python3-requests \
  p7zip-full \
  libfontconfig1 \
  libgl1-mesa-dev \
  libxcb-icccm4 \
  libxcb-image0 \
  libxcb-keysyms1 \
  libxcb-render-util0 \
  libxcb-xinerama0 \
  libxcb-xkb1 \
  libxkbcommon-x11-0 \
  libpq5 \
  libxcb-randr0 \
  libxcb-shape0 \
  libodbc1 \
  libxcb-xfixes0 \
  libegl1-mesa

# Force refresh ld.so.cache
ldconfig
SELF_DIR="$(dirname "$(readlink -f "${0}")")"

# install qt
if [ ! -d "${HOME}/Qt" ]; then
  curl -sSkL --compressed https://raw.githubusercontent.com/engnr/qt-downloader/master/qt-downloader | python3 - linux desktop 5.15.2 gcc_64 -o "${HOME}/Qt" -m qtbase qttools qtsvg icu
fi
export QT_BASE_DIR="$(ls -rd "${HOME}/Qt"/*/gcc_64 | head -1)"
export QTDIR=$QT_BASE_DIR
export PATH=$QT_BASE_DIR/bin:$PATH
export LD_LIBRARY_PATH=$QT_BASE_DIR/lib:$LD_LIBRARY_PATH
export PKG_CONFIG_PATH=$QT_BASE_DIR/lib/pkgconfig:$PKG_CONFIG_PATH
export QT_QMAKE="${QT_BASE_DIR}/bin"
sed -i.bak 's/Enterprise/OpenSource/g;s/licheck.*//g' "${QT_BASE_DIR}/mkspecs/qconfig.pri"

# build latest boost
mkdir -p /usr/src/boost
if [ ! -f /usr/src/boost/.unpack_ok ]; then
  boost_latest_url="$(curl -ksSfL https://www.boost.org/users/download/ | grep -o 'http[^"]*.tar.bz2' | head -1)"
  curl -ksSfL "${boost_latest_url}" | tar -jxf - -C /usr/src/boost --strip-components 1
fi
touch "/usr/src/boost/.unpack_ok"
cd /usr/src/boost
./bootstrap.sh
./b2 install --with-system variant=release

# build libtorrent-rasterbar
mkdir -p /usr/src/libtorrent-rasterbar
[ -f /usr/src/libtorrent-rasterbar/.unpack_ok ] ||
  curl -ksSfL https://github.com/arvidn/libtorrent/archive/RC_1_2.tar.gz |
  tar -zxf - -C /usr/src/libtorrent-rasterbar --strip-components 1
touch "/usr/src/libtorrent-rasterbar/.unpack_ok"
cd "/usr/src/libtorrent-rasterbar/"
CXXFLAGS="-std=c++17" CPPFLAGS="-std=c++17" ./bootstrap.sh --prefix=/usr --with-boost="/usr/local" --with-boost-libdir="/usr/local/lib" --disable-debug --disable-maintainer-mode --with-libiconv
make clean
make -j$(nproc)
make install

# build qbittorrent
cd "${SELF_DIR}/../../"
./configure --prefix=/tmp/qbee/AppDir/usr --with-boost="/usr/local" --with-boost-libdir="/usr/local/lib" CXXFLAGS="-std=c++17" CPPFLAGS="-std=c++17" || (cat config.log && exit 1)
make install -j$(nproc)

# build AppImage
[ -x "/tmp/linuxdeploy-x86_64.AppImage" ] || curl -LC- -o /tmp/linuxdeploy-x86_64.AppImage "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
[ -x "/tmp/linuxdeploy-plugin-qt-x86_64.AppImage" ] || curl -LC- -o /tmp/linuxdeploy-plugin-qt-x86_64.AppImage "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"
chmod -v +x '/tmp/linuxdeploy-plugin-qt-x86_64.AppImage' '/tmp/linuxdeploy-x86_64.AppImage'
# Fix run in docker, see: https://github.com/linuxdeploy/linuxdeploy/issues/86
# sed -i 's|AI\x02|\x00\x00\x00|' '/tmp/linuxdeploy-plugin-qt-x86_64.AppImage' '/tmp/linuxdeploy-x86_64.AppImage'
cd "/tmp/qbee"
mkdir -p "/tmp/qbee/AppDir/apprun-hooks/"
echo 'export XDG_DATA_DIRS="${APPDIR:-"$(dirname "${BASH_SOURCE[0]}")/.."}/usr/share:${XDG_DATA_DIRS}:/usr/share:/usr/local/share"' >"/tmp/qbee/AppDir/apprun-hooks/xdg_data_dirs.sh"
APPIMAGE_EXTRACT_AND_RUN=1 \
  OUTPUT='qBittorrent-Enhanced-Edition.AppImage' \
  UPDATE_INFORMATION="zsync|https://github.com/${GITHUB_REPOSITORY}/releases/latest/download/qBittorrent-Enhanced-Edition.AppImage.zsync" \
  /tmp/linuxdeploy-x86_64.AppImage --appdir="/tmp/qbee/AppDir" --output=appimage --plugin qt

cp -fv /tmp/qbee/qBittorrent-Enhanced-Edition.AppImage /tmp/qbee/qBittorrent-Enhanced-Edition.AppImage.zsync "${SELF_DIR}"
