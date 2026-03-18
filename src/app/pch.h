#pragma once

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

// qbt_base
#include "base/bittorrent/addtorrentparams.h"
#include "base/bittorrent/infohash.h"
#include "base/logger.h"
#include "base/settingvalue.h"

// Windows
#ifdef Q_OS_WIN
#include <windows.h>
#endif
