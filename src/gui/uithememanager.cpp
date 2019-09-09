/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Prince Gupta <jagannatharjun11@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If
 * you modify file(s), you may extend this exception to your version of the
 * file(s), but you are not obligated to do so. If you do not wish to do so,
 * delete this exception statement from your version.
 */

#include "uithememanager.h"

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>
#include <QResource>

#include "base/iconprovider.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"
#include "utils.h"

UIThemeManager *UIThemeManager::m_instance = nullptr;
static const char *allowedExts[] = {".svg", ".png"};


bool UIThemeManager::loadIconConfig(const QString &configFile, const QString &iconDir, UIThemeManager::IconMap &config)
{
    bool valid = false;
    QFile iconJsonMap(configFile);
    if (!iconJsonMap.open(QIODevice::ReadOnly)) {
        LogMsg(tr("Failed to open \"%1\", error: %2.")
               .arg(iconJsonMap.fileName(), iconJsonMap.errorString()), Log::WARNING);
        return false;
    }

    QJsonObject jsonObject;
    QJsonParseError err;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(iconJsonMap.readAll(), &err);
    if (err.error != QJsonParseError::NoError) {
        LogMsg(tr("Error occurred while parsing \"%1\", error: %2.")
               .arg(iconJsonMap.fileName(), err.errorString()), Log::WARNING);
    }
    else if (!jsonDoc.isObject()) {
        LogMsg(tr("Invalid icon configuration file format in \"%1\". JSON object is expected.")
               .arg(iconJsonMap.fileName()), Log::WARNING);
    }
    else {
        jsonObject = jsonDoc.object();
        valid = true;
    }

    if (!valid)
        return false;

    config.reserve(jsonObject.size());
    for (auto i = jsonObject.begin(), e = jsonObject.end(); i != e; i++) {
        if (!i.value().isString()) {
            LogMsg(tr("Error in iconconfig \"%1\", error: Provided value for %2, is not a string.")
                   .arg(configFile, i.key()), Log::WARNING);
            valid = false;
            continue;
        }

        QString iconPath = iconDir + i.value().toString();

        const char *ext = nullptr;
        if (Utils::Fs::fileExtension(iconPath).isEmpty()) {
            for (const char *e : allowedExts) {
                if (QFile::exists(iconPath + e)) {
                    ext = e;
                    break;
                }
            }
        }
        iconPath.append(ext);

        if (!QFile::exists(iconPath)) {
            LogMsg(tr(R"(Error in iconconfig "%1", error: Can't find file "%2" required in {'%3': '%4'})")
                   .arg(configFile, iconPath, i.key(), i.value().toString()), Log::WARNING);
            valid = false;
            continue;
        }

        config.insert(i.key(), iconPath);
    }

    return valid;
}

void UIThemeManager::freeInstance()
{
    if (m_instance) {
        delete m_instance;
        m_instance = nullptr;
    }
}

void UIThemeManager::initInstance()
{
    if (!m_instance)
        m_instance = new UIThemeManager;
}

UIThemeManager::UIThemeManager()
{
    const Preferences *const pref = Preferences::instance();
    m_useCustomUITheme = pref->useCustomUITheme();
    if (m_useCustomUITheme
        && !QResource::registerResource(pref->customUIThemePath(), "/uitheme")) {
        LogMsg(tr("Failed to load UI theme from file: \"%1\", falling back to system default theme.").arg(pref->customUIThemePath()), Log::WARNING);

        m_useCustomUITheme = false;
    }

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    m_useSystemTheme = pref->useSystemIconTheme();
    const char *iconConfigFile = m_useSystemTheme ? "systemiconconfig.json" : "iconconfig.json";
#else
    const char *iconConfigFile = "iconconfig.json";
#endif

    loadIconConfig(QString(":icons/") + iconConfigFile, ":icons/", m_iconMap);

    IconMap customIconMap;
    if (m_useCustomUITheme
        && loadIconConfig(QString(":uitheme/icons/") + iconConfigFile, ":uitheme/icons/", customIconMap)) {
        m_flagsDir = ":uitheme/icons/flags/";

        // add unique and replace existing values
        for (auto i = customIconMap.constBegin(), e = customIconMap.constEnd(); i != e; i++)
            m_iconMap.insert(i.key(), i.value());
    }
    else {
        m_flagsDir = ":icons/flags/";
    }
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    if (!m_useCustomUITheme) {
        qApp->setStyleSheet({});
        return;
    }

    QFile qssFile(":uitheme/stylesheet.qss");
    if (!qssFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qApp->setStyleSheet({});
        LogMsg(tr("Couldn't apply theme stylesheet. stylesheet.qss couldn't be opened. Reason: %1").arg(qssFile.errorString())
               , Log::WARNING);
        return;
    }

    qApp->setStyleSheet(qssFile.readAll());
}

QIcon UIThemeManager::getIcon(const QString &iconId) const
{
    return getIcon(iconId, iconId);
}

QIcon UIThemeManager::getIcon(const QString &iconId, const QString &fallback) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme) {
        QString themeIcon = m_iconMap.value(iconId);
        QIcon icon = QIcon::fromTheme(themeIcon);
        if (icon.name() != themeIcon)
            icon = QIcon::fromTheme(fallback, QIcon(getIconPath(iconId)));
        return icon;
    }
#else
    Q_UNUSED(fallback)
#endif
    // cache to avoid rescaling svg icons
    static QHash<QString, QIcon> iconCache;
    const QString iconPath = getIconPath(iconId);
    const auto iter = iconCache.find(iconPath);
    if (iter != iconCache.end())
        return *iter;

    const QIcon icon {iconPath};
    iconCache[iconPath] = icon;
    return icon;
}

QIcon UIThemeManager::getFlagIcon(const QString &countryIsoCode) const
{
    if (countryIsoCode.isEmpty()) return {};
    return QIcon(m_flagsDir + countryIsoCode.toLower() + ".svg");
}

QPixmap UIThemeManager::getScaledPixmap(const QString &iconId, const QWidget *widget, int baseHeight) const
{
    return Utils::Gui::scaledPixmapSvg(getIconPath(iconId), widget, baseHeight);
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (m_useSystemTheme) {
        QString path = Utils::Fs::tempPath() + iconId + ".png";
        if (!QFile::exists(path)) {
            const QIcon icon = QIcon::fromTheme(iconId);
            if (!icon.isNull())
                icon.pixmap(32).save(path);
            else
                path = IconProvider::instance()->getIconPath(iconId);
        }

        return path;
    }
#endif

    QString iconPath = m_iconMap.value(iconId);
    QString iconIdPattern = iconId;
    while (iconPath.isEmpty() && !iconIdPattern.isEmpty()) {
        const int sepIndex = iconIdPattern.indexOf('.');
        iconIdPattern = "*" + ((sepIndex == -1) ? "" : iconIdPattern.right(iconIdPattern.size() - sepIndex));
        const auto patternIter = m_iconMap.find(iconIdPattern);
        if (patternIter != m_iconMap.end())
            iconPath = *patternIter;
        else
            iconIdPattern.remove(0, 2); // removes "*."
    }

    if (iconPath.isEmpty()) {
        LogMsg(tr("Can't resolve icon id - %1").arg(iconId), Log::WARNING);
        return {};
    }

    return iconPath;
}
