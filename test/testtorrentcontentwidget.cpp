/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  The qBittorrent project
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 */

#include <QAccessible>
#include <QFuture>
#include <QItemSelectionModel>
#include <QTest>
#include <QTemporaryDir>

#include "base/bittorrent/torrentcontenthandler.h"
#include "base/path.h"
#include "base/preferences.h"
#include "base/profile.h"
#include "base/settingsstorage.h"
#include "gui/torrentcontentmodel.h"
#include "gui/torrentcontentmodelitem.h"
#include "gui/torrentcontentwidget.h"
#include "gui/uithememanager.h"

namespace
{
    class TestTorrentContentHandler final : public BitTorrent::TorrentContentHandler
    {
    public:
        bool hasMetadata() const override { return true; }
        int filesCount() const override { return 1; }
        Path filePath(const int index) const override { return Path(QStringLiteral("video.mp4%1").arg(index)); }
        qlonglong fileSize([[maybe_unused]] const int index) const override { return 1200000000; }
        Path actualStorageLocation() const override { return {}; }
        Path actualFilePath(const int index) const override { return filePath(index); }
        QList<BitTorrent::DownloadPriority> filePriorities() const override { return {BitTorrent::DownloadPriority::Ignored}; }
        QList<qreal> filesProgress() const override { return {0}; }
        QFuture<QList<qreal>> fetchAvailableFileFractions() const override { return {}; }
        void renameFile([[maybe_unused]] int index, [[maybe_unused]] const Path &newPath) override {}
        void prioritizeFiles(const QList<BitTorrent::DownloadPriority> &priorities) override { m_priorities = priorities; }
        void flushCache() const override {}

    protected:
        void doRenameFolder([[maybe_unused]] const Path &oldPath, [[maybe_unused]] const Path &newPath) override {}

    private:
        QList<BitTorrent::DownloadPriority> m_priorities;
    };
}

class TestTorrentContentWidget final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestTorrentContentWidget)

public:
    TestTorrentContentWidget() = default;

private slots:
    void initTestCase()
    {
        QVERIFY(m_profileDir.isValid());
        Profile::initInstance(Path(m_profileDir.path()), QStringLiteral("test"), false);
        SettingsStorage::initInstance();
        Preferences::initInstance();
        UIThemeManager::initInstance();
    }

    void cleanupTestCase()
    {
        UIThemeManager::freeInstance();
        Preferences::freeInstance();
        SettingsStorage::freeInstance();
        Profile::freeInstance();
    }

    void priorityChangeDoesNotExposeTransientSelection()
    {
        TestTorrentContentHandler contentHandler;
        TorrentContentWidget widget;
        widget.setContentHandler(&contentHandler);
        widget.show();
        QVERIFY(QTest::qWaitForWindowExposed(&widget));

        const QModelIndex index = widget.model()->index(0, TorrentContentModelItem::COL_PRIO);
        QVERIFY(index.isValid());
        widget.selectionModel()->select(index, QItemSelectionModel::Select | QItemSelectionModel::Rows);

        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(&widget);
        QVERIFY(accessible);
        QAccessibleSelectionInterface *selection = accessible->selectionInterface();
        QVERIFY(selection);
        QCOMPARE(selection->selectedItems().size(), TorrentContentModelItem::NB_COL);

        bool selectionWasCleared = false;
        bool selectionWasRestored = false;
        const auto *contentModel = widget.findChild<TorrentContentModel *>();
        QVERIFY(contentModel);
        connect(contentModel, &TorrentContentModel::priorityUpdateStarted, this, [&widget, &selectionWasCleared]
        {
            selectionWasCleared = widget.selectionModel()->selectedIndexes().isEmpty();
        });
        connect(contentModel, &TorrentContentModel::priorityUpdateFinished, this, [&widget, &selectionWasRestored]
        {
            selectionWasRestored = (widget.selectionModel()->selectedIndexes().size() == TorrentContentModelItem::NB_COL);
        });

        QVERIFY(widget.model()->setData(index, static_cast<int>(BitTorrent::DownloadPriority::Normal)));
        QVERIFY(selectionWasCleared);
        QVERIFY(selectionWasRestored);
        QCOMPARE(selection->selectedItems().size(), TorrentContentModelItem::NB_COL);
    }

private:
    QTemporaryDir m_profileDir;
};

QTEST_MAIN(TestTorrentContentWidget)
#include "testtorrentcontentwidget.moc"
