#pragma once

// std
#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <io.h>
#include <memory>
#include <optional>
#include <string>

// Windows
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// Qt
#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QList>
#include <QMessageBox>
#include <QMetaEnum>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QTimer>
#include <QtSystemDetection>

// libtorrent
#include "libtorrent/add_torrent_params.hpp"
#include "libtorrent/info_hash.hpp"
