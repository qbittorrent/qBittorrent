/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2019  Mike Tzou (Chocobo1)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "upgrade.h"

#include <QFile>

#include "base/logger.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "base/utils/fs.h"

namespace
{
    void exportWebUIHttpsFiles()
    {
        const auto migrate = [](const QString &oldKey, const QString &newKey, const QString &savePath) {
            SettingsStorage *settingsStorage {SettingsStorage::instance()};
            const QByteArray oldData {settingsStorage->loadValue(oldKey).toByteArray()};
            const QString newData {settingsStorage->loadValue(newKey).toString()};
            const QString errorMsgFormat {QObject::tr("Migrate preferences failed: WebUI https, file: \"%1\", error: \"%2\"")};

            if (!newData.isEmpty() || oldData.isEmpty())
                return;

            QFile file(savePath);
            if (!file.open(QIODevice::WriteOnly)) {
                LogMsg(errorMsgFormat.arg(savePath, file.errorString()) , Log::WARNING);
                return;
            }
            if (file.write(oldData) != oldData.size()) {
                file.close();
                Utils::Fs::forceRemove(savePath);
                LogMsg(errorMsgFormat.arg(savePath, QLatin1String("Write incomplete.")) , Log::WARNING);
                return;
            }

            settingsStorage->storeValue(newKey, savePath);
            settingsStorage->removeValue(oldKey);

            LogMsg(QObject::tr("Migrated preferences: WebUI https, exported data to file: \"%1\"").arg(savePath)
                , Log::INFO);
        };

        const QString configPath {specialFolderLocation(SpecialFolder::Config)};
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Certificate")
            , QLatin1String("Preferences/WebUI/HTTPS/CertificatePath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUICertificate.crt")));
        migrate(QLatin1String("Preferences/WebUI/HTTPS/Key")
            , QLatin1String("Preferences/WebUI/HTTPS/KeyPath")
            , Utils::Fs::toNativePath(configPath + QLatin1String("WebUIPrivateKey.pem")));
    }
}

bool upgrade(const bool /*ask*/)
{
    exportWebUIHttpsFiles();
    return true;
}

void handleChangedDefaults(const DefaultPreferencesMode mode)
{
    struct DefaultValue
    {
        QString name;
        QVariant legacy;
        QVariant current;
    };

    const QVector<DefaultValue> changedDefaults {
        {QLatin1String {"BitTorrent/Session/QueueingSystemEnabled"}, true, false}
    };

    SettingsStorage *settingsStorage {SettingsStorage::instance()};
    for (auto it = changedDefaults.cbegin(); it != changedDefaults.cend(); ++it) {
        if (settingsStorage->loadValue(it->name).isNull())
            settingsStorage->storeValue(it->name, (mode == DefaultPreferencesMode::Legacy ? it->legacy : it->current));
    }
}
