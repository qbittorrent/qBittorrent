/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  qBittorrent contributors
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

#include <algorithm>

#include <QAccessible>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include "gui/properties/proptabbar.h"

namespace
{
    QList<QPushButton *> buttons(const PropTabBar &tabBar)
    {
        QList<QPushButton *> result;
        for (int i = 0; i < tabBar.count(); ++i)
        {
            if (auto *button = qobject_cast<QPushButton *>(tabBar.itemAt(i)->widget()))
                result.append(button);
        }
        return result;
    }

    int checkedButtonCount(const QList<QPushButton *> &buttons)
    {
        return std::count_if(buttons.cbegin(), buttons.cend()
                , [](const QPushButton *button) { return button->isChecked(); });
    }

    void show(QWidget &widget)
    {
        widget.show();
        QTest::qWait(0);
    }
}

class TestPropTabBar final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(TestPropTabBar)

public:
    TestPropTabBar() = default;

private slots:
    void testAccessibleSelectionState() const
    {
        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        QCOMPARE(tabButtons.size(), 6);

        tabBar.setCurrentIndex(PropTabBar::MainTab);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::MainTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);
        QVERIFY(tabButtons[PropTabBar::MainTab]->isChecked());

        for (QPushButton *button : tabButtons)
        {
            QVERIFY(button->isCheckable());
            QVERIFY(button->autoExclusive());

            const QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(button);
            QVERIFY(interface);
            QCOMPARE(interface->role(), QAccessible::RadioButton);
            QCOMPARE(interface->state().checked, button->isChecked());
        }
    }

    void testMouseInteraction() const
    {
        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        QSignalSpy tabChangedSpy(&tabBar, &PropTabBar::tabChanged);
        QSignalSpy visibilitySpy(&tabBar, &PropTabBar::visibilityToggled);

        QTest::mouseClick(tabButtons[PropTabBar::MainTab], Qt::LeftButton);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::MainTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);

        QTest::mouseClick(tabButtons[PropTabBar::TrackersTab], Qt::LeftButton);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::TrackersTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);

        QTest::mouseClick(tabButtons[PropTabBar::TrackersTab], Qt::LeftButton);
        QCOMPARE(tabBar.currentIndex(), -1);
        QCOMPARE(checkedButtonCount(tabButtons), 0);

        QTest::mouseClick(tabButtons[PropTabBar::TrackersTab], Qt::LeftButton);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::TrackersTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);

        QCOMPARE(tabChangedSpy.count(), 3);
        QCOMPARE(tabChangedSpy.at(0).at(0).toInt(), PropTabBar::MainTab);
        QCOMPARE(tabChangedSpy.at(1).at(0).toInt(), PropTabBar::TrackersTab);
        QCOMPARE(tabChangedSpy.at(2).at(0).toInt(), PropTabBar::TrackersTab);
        QCOMPARE(visibilitySpy.count(), 3);
        QCOMPARE(visibilitySpy.at(0).at(0).toBool(), true);
        QCOMPARE(visibilitySpy.at(1).at(0).toBool(), false);
        QCOMPARE(visibilitySpy.at(2).at(0).toBool(), true);
    }

    void testAccessibleToggle() const
    {
        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        tabBar.setCurrentIndex(PropTabBar::MainTab);
        QSignalSpy tabChangedSpy(&tabBar, &PropTabBar::tabChanged);
        QSignalSpy visibilitySpy(&tabBar, &PropTabBar::visibilityToggled);

        for (const int index : {PropTabBar::TrackersTab, PropTabBar::PeersTab})
        {
            QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(tabButtons[index]);
            QVERIFY(interface);
            QAccessibleActionInterface *actionInterface = interface->actionInterface();
            QVERIFY(actionInterface);
            QVERIFY(actionInterface->actionNames().contains(QAccessibleActionInterface::toggleAction()));

            actionInterface->doAction(QAccessibleActionInterface::toggleAction());
            QCOMPARE(tabBar.currentIndex(), index);
            QCOMPARE(checkedButtonCount(tabButtons), 1);
            QVERIFY(tabButtons[index]->isChecked());
        }

        QCOMPARE(tabChangedSpy.count(), 2);
        QCOMPARE(visibilitySpy.count(), 0);
    }
};

QTEST_MAIN(TestPropTabBar)
#include "testproptabbar.moc"
