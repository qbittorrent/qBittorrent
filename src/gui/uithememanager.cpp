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
#include <QResource>

#include "base/iconprovider.h"
#include "base/logger.h"
#include "base/preferences.h"
#include "base/utils/fs.h"

UIThemeManager *UIThemeManager::m_instance = nullptr;

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
    bool useCustomUITheme = pref->useCustomUITheme();
    if (useCustomUITheme
        && !QResource::registerResource(pref->customUIThemePath(), "/uitheme")) {
        LogMsg(tr("Failed to load UI theme from file: \"%1\", falling back to system default theme").arg(pref->customUIThemePath()), Log::WARNING);

        useCustomUITheme = false;
        Preferences::instance()->setUseCustomUITheme(false);
    }

#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    m_useSystemTheme = pref->useSystemIconTheme();
#endif

    m_iconsDir = useCustomUITheme ? ":uitheme/icons/" : ":icons/";

    QFile iconJsonMap(m_iconsDir + "iconconfig.json");
    if (!iconJsonMap.open(QIODevice::ReadOnly))
        LogMsg(tr("Failed to open \"%1\", error: %2").arg(iconJsonMap.fileName(), iconJsonMap.errorString()), Log::WARNING);

    QJsonParseError err;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(iconJsonMap.readAll(), &err);
    if (err.error != QJsonParseError::NoError)
        LogMsg(tr("Error occurred while parsing \"%1\", error: %2").arg(iconJsonMap.fileName(), err.errorString()), Log::WARNING);
    if (!jsonDoc.isObject())
        LogMsg(tr("Json Document isn't an object."), Log::WARNING);
    m_iconMap = jsonDoc.object();
}

UIThemeManager *UIThemeManager::instance()
{
    return m_instance;
}

void UIThemeManager::applyStyleSheet() const
{
    if (!Preferences::instance()->useCustomUITheme()) {
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
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
    if (m_useSystemTheme) {
        QIcon icon = QIcon::fromTheme(iconId);
        if (icon.name() != iconId)
            icon = QIcon::fromTheme(fallback, QIcon(IconProvider::instance()->getIconPath(iconId)));
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
    return QIcon(m_iconsDir + "flags/" + countryIsoCode.toLower() + ".svg");
}

QString UIThemeManager::getIconPath(const QString &iconId) const
{
#if (defined(Q_OS_UNIX) && !defined(Q_OS_MAC))
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

    const auto iter = m_iconMap.find(iconId);
    QString iconPath = (iter != m_iconMap.end()) ? iter->toString() : QString{};
    QString iconIdPattern = iconId;
    while (iconPath.isEmpty() && !iconIdPattern.isEmpty()) {
        const int sepIndex = iconIdPattern.indexOf('.');
        iconIdPattern = "*" + ((sepIndex == -1) ? "" : iconIdPattern.right(iconIdPattern.size() - sepIndex));
        const auto patternIter = m_iconMap.find(iconIdPattern);
        if (patternIter != m_iconMap.end())
            iconPath = patternIter->toString();
        else
            iconIdPattern.remove(0, 2); // removes "*."
    }

    if (iconPath.isEmpty()) {
        LogMsg(tr("Can't resolve icon id - %1").arg(iconId), Log::WARNING);
        return {};
    }

    m_iconMap[iconId] = iconPath;
    iconPath.prepend(m_iconsDir);

    if (iconPath.lastIndexOf('.') != -1) {
        // resolved name already has an extension
        if (QFile::exists(iconPath))
            return iconPath;
        LogMsg(tr("Icon Path \"%1\" doesn't exists").arg(iconPath), Log::WARNING);
        return {};
    }

    const QString allowedExts[] = {".svg", ".png"};
    for (const QString &ext : allowedExts) {
        QFile f(iconPath + ext);
        if (f.exists())
            return f.fileName();
    }

    LogMsg(tr("Can't match \"%1\" with any of the allowed extensions").arg(iconPath), Log::WARNING);
    return {};
}
