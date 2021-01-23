#!/bin/sh -e
# This scrip is for cross compilations
# Please run this scrip in docker image: alpine:latest
# E.g: docker run -e CROSS_HOST=arm-linux-musleabi -e OPENSSL_COMPILER=linux-armv4 -e QT_DEVICE=linux-arm-generic-g++ --rm -v `git rev-parse --show-toplevel`:/build alpine /build/.github/workflows/cross_build.sh
# Artifacts will copy to the same directory.

# alpine repository mirror for local building
# sed -i 's/dl-cdn.alpinelinux.org/mirrors.aliyun.com/' /etc/apk/repositories

# value from: https://musl.cc/ (without -cross or -native)
export CROSS_HOST="${CROSS_HOST:-arm-linux-musleabi}"
# value from openssl source: ./Configure LIST
export OPENSSL_COMPILER="${OPENSSL_COMPILER:-linux-armv4}"
# value from https://github.com/qt/qtbase/tree/dev/mkspecs/
export QT_XPLATFORM="${QT_XPLATFORM}"
# value from https://github.com/qt/qtbase/tree/dev/mkspecs/devices/
export QT_DEVICE="${QT_DEVICE}"
# match qt version prefix. E.g 5 --> 5.15.2, 5.12 --> 5.12.10
export QT_VER_PREFIX="5"
export LIBTORRENT_BRANCH="RC_1_2"
export CROSS_ROOT="${CROSS_ROOT:-/cross_root}"

apk add gcc \
  g++ \
  make \
  file \
  perl \
  autoconf \
  automake \
  libtool \
  tar \
  jq \
  pkgconfig \
  linux-headers \
  zip \
  xz

TARGET_ARCH="${CROSS_HOST%%-*}"
TARGET_HOST="${CROSS_HOST#*-}"
case "${TARGET_HOST}" in
*"mingw"*)
  TARGET_HOST=win
  apk add wine
  export WINEPREFIX=/tmp/
  RUNNER_CHECKER="wine64"
  ;;
*)
  TARGET_HOST=linux
  apk add "qemu-${TARGET_ARCH}"
  RUNNER_CHECKER="qemu-${TARGET_ARCH}"
  ;;
esac

export PATH="${CROSS_ROOT}/bin:${PATH}"
export CROSS_PREFIX="${CROSS_ROOT}/${CROSS_HOST}"
export PKG_CONFIG_PATH="${CROSS_PREFIX}/opt/qt/lib/pkgconfig:${CROSS_PREFIX}/lib/pkgconfig:${PKG_CONFIG_PATH}"
SELF_DIR="$(dirname "$(readlink -f "${0}")")"

mkdir -p "${CROSS_ROOT}" \
  /usr/src/zlib \
  /usr/src/openssl \
  /usr/src/boost \
  /usr/src/libiconv \
  /usr/src/libtorrent \
  /usr/src/qtbase \
  /usr/src/qttools

# toolchain
if [ ! -f "${SELF_DIR}/${CROSS_HOST}-cross.tgz" ]; then
  wget -c -O "${SELF_DIR}/${CROSS_HOST}-cross.tgz" "https://musl.cc/${CROSS_HOST}-cross.tgz"
fi
tar -zxf "${SELF_DIR}/${CROSS_HOST}-cross.tgz" --transform='s|^\./||S' --strip-components=1 -C "${CROSS_ROOT}"
# mingw does not contains posix thread support: https://github.com/meganz/mingw-std-threads
if [ "${TARGET_HOST}" = 'win' ]; then
  if [ ! -f "${SELF_DIR}/mingw-std-threads.tar.gz" ]; then
    wget -c -O "${SELF_DIR}/mingw-std-threads.tar.gz" "https://github.com/meganz/mingw-std-threads/archive/master.tar.gz"
  fi
  mkdir -p /usr/src/mingw-std-threads/
  tar -zxf "${SELF_DIR}/mingw-std-threads.tar.gz" --strip-components=1 -C "/usr/src/mingw-std-threads/"
  cp -fv /usr/src/mingw-std-threads/*.h "${CROSS_PREFIX}/include"
fi

# zlib
if [ ! -f "${SELF_DIR}/zlib.tar.gz" ]; then
  zlib_latest_url="$(wget -qO- https://api.github.com/repos/madler/zlib/tags | jq -r '.[0].tarball_url')"
  wget -c -O "${SELF_DIR}/zlib.tar.gz" "${zlib_latest_url}"
fi
tar -zxf "${SELF_DIR}/zlib.tar.gz" --strip-components=1 -C /usr/src/zlib
cd /usr/src/zlib
if [ "${TARGET_HOST}" = win ]; then
  make -f win32/Makefile.gcc BINARY_PATH="${CROSS_PREFIX}/bin" INCLUDE_PATH="${CROSS_PREFIX}/include" LIBRARY_PATH="${CROSS_PREFIX}/lib" SHARED_MODE=0 PREFIX="${CROSS_HOST}-" -j$(nproc) install
else
  CHOST="${CROSS_HOST}" ./configure --prefix="${CROSS_PREFIX}" --static
  make -j$(nproc)
  make install
fi

# openssl
if [ ! -f "${SELF_DIR}/openssl.tar.gz" ]; then
  openssl_filename="$(wget -qO- https://www.openssl.org/source/ | grep -o 'href="openssl-1.*tar.gz"' | grep -o '[^"]*.tar.gz')"
  openssl_latest_url="https://www.openssl.org/source/${openssl_filename}"
  wget -c -O "${SELF_DIR}/openssl.tar.gz" "${openssl_latest_url}"
fi
tar -zxf "${SELF_DIR}/openssl.tar.gz" --strip-components=1 -C /usr/src/openssl
cd /usr/src/openssl
./Configure -static --cross-compile-prefix="${CROSS_HOST}-" --prefix="${CROSS_PREFIX}" "${OPENSSL_COMPILER}"
make depend
make -j$(nproc)
make install_sw

# boost
if [ ! -f "${SELF_DIR}/boost.tar.bz2" ]; then
  boost_latest_url="$(wget -qO- https://www.boost.org/users/download/ | grep -o 'http[^"]*.tar.bz2' | head -1)"
  wget -c -O "${SELF_DIR}/boost.tar.bz2" "${boost_latest_url}"
fi
tar -jxf "${SELF_DIR}/boost.tar.bz2" --strip-components=1 -C /usr/src/boost
cd /usr/src/boost
./bootstrap.sh
sed -i "s/using gcc.*/using gcc : cross : ${CROSS_HOST}-g++ ;/" project-config.jam
./b2 install --prefix="${CROSS_PREFIX}" --with-system toolset=gcc-cross variant=release link=static runtime-link=static

# qt
qt_major_ver="$(wget -qO- https://download.qt.io/official_releases/qt/ | sed -nr 's@.*href="([0-9]+(\.[0-9]+)*)/".*@\1@p' | grep "^${QT_VER_PREFIX}" | tail -1)"
qt_ver="$(wget -qO- https://download.qt.io/official_releases/qt/${qt_major_ver}/ | sed -nr 's@.*href="([0-9]+(\.[0-9]+)*)/".*@\1@p' | grep "^${QT_VER_PREFIX}" | tail -1)"
echo "Using qt version: ${qt_ver}"
qtbase_url="https://download.qt.io/official_releases/qt/${qt_major_ver}/${qt_ver}/submodules/qtbase-everywhere-src-${qt_ver}.tar.xz"
qtbase_filename="qtbase-everywhere-src-${qt_ver}.tar.xz"
qttools_url="https://download.qt.io/official_releases/qt/${qt_major_ver}/${qt_ver}/submodules/qttools-everywhere-src-${qt_ver}.tar.xz"
qttools_filename="qttools-everywhere-src-${qt_ver}.tar.xz"
if [ ! -f "${SELF_DIR}/${qtbase_filename}" ]; then
  wget -c -O "${SELF_DIR}/${qtbase_filename}" "${qtbase_url}"
fi
if [ ! -f "${SELF_DIR}/${qttools_filename}" ]; then
  wget -c -O "${SELF_DIR}/${qttools_filename}" "${qttools_url}"
fi
tar -Jxf "${SELF_DIR}/${qtbase_filename}" --strip-components=1 -C /usr/src/qtbase
tar -Jxf "${SELF_DIR}/${qttools_filename}" --strip-components=1 -C /usr/src/qttools
cd /usr/src/qtbase
# Remove some options no support by this toolchain
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-fno-fat-lto-objects//g'
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-fuse-linker-plugin//g'
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-mfloat-abi=softfp//g'
if [ "${TARGET_HOST}" = 'win' ]; then
  export OPENSSL_LIBS="-lssl -lcrypto -lcrypt32 -lws2_32"
  # musl.cc x86_64-w64-mingw32 toolchain not supports thread local
  sed -i '/define\s*Q_COMPILER_THREAD_LOCAL/d' src/corelib/global/qcompilerdetection.h
fi
./configure --prefix=/opt/qt/ -optimize-size -silent --openssl-linked \
  -static -opensource -confirm-license -release -c++std c++17 -no-opengl \
  -no-dbus -no-widgets -no-gui -no-compile-examples -ltcg -make libs -no-pch \
  -nomake tests -nomake examples -no-xcb -no-feature-testlib \
  -hostprefix "${CROSS_ROOT}" ${QT_XPLATFORM:+-xplatform "${QT_XPLATFORM}"} \
  ${QT_DEVICE:+-device "${QT_DEVICE}"} -device-option CROSS_COMPILE="${CROSS_HOST}-" \
  -sysroot "${CROSS_PREFIX}"
make -j$(nproc)
make install
cd /usr/src/qttools
qmake -set prefix "${CROSS_ROOT}"
qmake
# Remove some options no support by this toolchain
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-fno-fat-lto-objects//g'
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-fuse-linker-plugin//g'
find -name '*.conf' -print0 | xargs -0 -r sed -i 's/-mfloat-abi=softfp//g'
make -j$(nproc) install
cd "${CROSS_ROOT}/bin"
ln -sf lrelease "lrelease-qt${qt_ver:1:1}"

# libiconv
if [ ! -f "${SELF_DIR}/libiconv.tar.gz" ]; then
  libiconv_latest_url="$(wget -qO- https://www.gnu.org/software/libiconv/ | grep -o '[^>< "]*ftp.gnu.org/pub/gnu/libiconv/.[^>< "]*' | head -1)"
  wget -c -O "${SELF_DIR}/libiconv.tar.gz" "${libiconv_latest_url}"
fi
tar -zxf "${SELF_DIR}/libiconv.tar.gz" --strip-components=1 -C /usr/src/libiconv/
cd /usr/src/libiconv/
./configure CXXFLAGS="-std=c++17" --host="${CROSS_HOST}" --prefix="${CROSS_PREFIX}" --enable-static --disable-shared --enable-silent-rules
make -j$(nproc)
make install

# libtorrent
if [ ! -f "${SELF_DIR}/libtorrent.tar.gz" ]; then
  wget -c -O "${SELF_DIR}/libtorrent.tar.gz" "https://github.com/arvidn/libtorrent/archive/${LIBTORRENT_BRANCH}.tar.gz"
fi
tar -zxf "${SELF_DIR}/libtorrent.tar.gz" --strip-components=1 -C /usr/src/libtorrent
cd /usr/src/libtorrent
if [ "${TARGET_HOST}" = 'win' ]; then
  export LIBS="-lcrypt32 -lws2_32"
  # musl.cc x86_64-w64-mingw32 toolchain not supports thread local
  export CPPFLAGS='-D_WIN32_WINNT=0x0602 -DBOOST_NO_CXX11_THREAD_LOCAL'
fi
./bootstrap.sh CXXFLAGS="-std=c++17" --host="${CROSS_HOST}" --prefix="${CROSS_PREFIX}" --enable-static --disable-shared --enable-silent-rules --with-boost="${CROSS_PREFIX}" --with-libiconv
# fix x86_64-w64-mingw32 build
if [ "${TARGET_HOST}" = 'win' ]; then
  find -type f \( -name '*.cpp' -o -name '*.hpp' \) -print0 |
    xargs -0 -r sed -i 's/include\s*<condition_variable>/include "mingw.condition_variable.h"/g;
                        s/include\s*<future>/include "mingw.future.h"/g;
                        s/include\s*<invoke>/include "mingw.invoke.h"/g;
                        s/include\s*<mutex>/include "mingw.mutex.h"/g;
                        s/include\s*<shared_mutex>/include "mingw.shared_mutex.h"/g;
                        s/include\s*<thread>/include "mingw.thread.h"/g'
fi
make -j$(nproc)
make install
unset LIBS CPPFLAGS

# build qbittorrent
cd "${SELF_DIR}/../../"
if [ "${TARGET_HOST}" = 'win' ]; then
  find \( -name '*.cpp' -o -name '*.h' \) -type f -print0 |
    xargs -0 -r sed -i 's/Windows\.h/windows.h/g;
      s/Shellapi\.h/shellapi.h/g;
      s/Shlobj\.h/shlobj.h/g;
      s/Ntsecapi\.h/ntsecapi.h/g'
  export LIBS="-lmswsock"
  export CPPFLAGS='-std=c++17 -D_WIN32_WINNT=0x0602'
fi
LIBS="${LIBS} -liconv" ./configure --host="${CROSS_HOST}" --prefix="${CROSS_PREFIX}" --disable-gui --with-boost="${CROSS_PREFIX}" CXXFLAGS="-std=c++17 ${CPPFLAGS}" LDFLAGS='-s -static --static'
make -j$(nproc)
make install
unset LIBS CPPFLAGS
if [ "${TARGET_HOST}" = 'win' ]; then
  cp -fv "src/release/qbittorrent-nox.exe" /tmp/
else
  cp -fv "${CROSS_PREFIX}/bin/qbittorrent-nox" /tmp/
fi

# check
"${RUNNER_CHECKER}" /tmp/qbittorrent-nox* --version 2>/dev/null

# archive qbittorrent
zip -j9v "${SELF_DIR}/qbittorrent-nox_${CROSS_HOST}_static.zip" /tmp/qbittorrent-nox*
