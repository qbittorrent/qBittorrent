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
#include <QGuiApplication>
#include <QImage>
#include <QPalette>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <QWidget>

#include "gui/properties/proptabbar.h"
#ifdef Q_OS_MACOS
#include "testproptabbar_macos.h"
#endif

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

#ifdef Q_OS_MACOS
    bool activateWindow(QWidget &widget)
    {
        TestMacOS::activateApplication();
        widget.raise();
        widget.activateWindow();

        for (int i = 0; (i < 100) && !widget.isActiveWindow(); ++i)
            QTest::qWait(10);

        return widget.isActiveWindow();
    }

    QImage buttonImage(QPushButton &button)
    {
        return button.grab().toImage();
    }

    QColor buttonFaceColor(const QImage &image)
    {
        return image.pixelColor(image.width() / 6, image.height() / 2);
    }

    qreal colorDistance(const QColor &lhs, const QColor &rhs)
    {
        return std::max({qAbs(lhs.redF() - rhs.redF()), qAbs(lhs.greenF() - rhs.greenF())
                , qAbs(lhs.blueF() - rhs.blueF()), qAbs(lhs.alphaF() - rhs.alphaF())});
    }

    QColor compositeOver(const QColor &foreground, const QColor &background)
    {
        const qreal foregroundAlpha = foreground.alphaF();
        return QColor::fromRgbF(
                (foreground.redF() * foregroundAlpha) + (background.redF() * (1 - foregroundAlpha)),
                (foreground.greenF() * foregroundAlpha) + (background.greenF() * (1 - foregroundAlpha)),
                (foreground.blueF() * foregroundAlpha) + (background.blueF() * (1 - foregroundAlpha)));
    }

    bool containsColor(const QImage &image, const QColor &color, const qreal tolerance)
    {
        for (int y = 0; y < image.height(); ++y)
        {
            for (int x = 0; x < image.width(); ++x)
            {
                if (colorDistance(image.pixelColor(x, y), color) <= tolerance)
                    return true;
            }
        }
        return false;
    }
#endif

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
#ifdef Q_OS_MACOS
    void testSelectedVisualUsesSystemColors_data() const
    {
        QTest::addColumn<bool>("darkAppearance");
        QTest::newRow("light") << false;
        QTest::newRow("dark") << true;
    }

    void testSelectedVisualUsesSystemColors() const
    {
        if (QGuiApplication::platformName() != QStringLiteral("cocoa"))
            QSKIP("The macOS accent regression requires the Cocoa platform plugin");

        QFETCH(const bool, darkAppearance);
        TestMacOS::setApplicationAppearance(darkAppearance);
        QCOMPARE(TestMacOS::isDarkAppearance(), darkAppearance);

        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);
        if (!activateWindow(parent))
            QSKIP("The Cocoa platform could not activate the test window");

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        tabBar.setCurrentIndex(PropTabBar::MainTab);

        QPushButton *selectedButton = tabButtons[PropTabBar::MainTab];
        QPushButton *unselectedButton = tabButtons[PropTabBar::TrackersTab];
        const QImage selectedImage = buttonImage(*selectedButton);
        const QColor selectedFace = buttonFaceColor(selectedImage);
        const QColor unselectedFace = buttonFaceColor(buttonImage(*unselectedButton));
        const QColor controlAccentColor = TestMacOS::controlAccentColor();

        QVERIFY(selectedFace != unselectedFace);
        QVERIFY(colorDistance(selectedFace, controlAccentColor) < 0.03);
        QVERIFY(containsColor(selectedImage, TestMacOS::alternateSelectedControlTextColor(), 0.03));

        QWidget otherWindow;
        show(otherWindow);
        otherWindow.activateWindow();
        QTRY_VERIFY(otherWindow.isActiveWindow());
        QTRY_VERIFY(!parent.isActiveWindow());

        const QImage inactiveSelectedImage = buttonImage(*selectedButton);
        const QColor inactiveSelectedFace = buttonFaceColor(inactiveSelectedImage);
        const QColor inactiveUnselectedFace = buttonFaceColor(buttonImage(*unselectedButton));
        const QColor inactiveHighlight = selectedButton->palette().color(QPalette::Inactive, QPalette::Highlight);
        const QColor inactiveHighlightedText = selectedButton->palette()
                .color(QPalette::Inactive, QPalette::HighlightedText);
        QVERIFY(inactiveSelectedFace != inactiveUnselectedFace);
        QVERIFY(colorDistance(inactiveSelectedFace, inactiveHighlight) < 0.01);
        QVERIFY(containsColor(inactiveSelectedImage
                , compositeOver(inactiveHighlightedText, inactiveHighlight), 0.03));
    }

    void cleanup() const
    {
        TestMacOS::resetApplicationAppearance();
    }
#endif

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

    void testReleaseOutsideKeepsSelectionExclusive() const
    {
        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        QTest::mouseClick(tabButtons[PropTabBar::MainTab], Qt::LeftButton);

        QTest::mousePress(tabButtons[PropTabBar::MainTab], Qt::LeftButton);
        tabButtons[PropTabBar::MainTab]->releaseMouse();
        QTest::mouseMove(&parent, parent.rect().bottomRight());
        QTest::mouseRelease(&parent, Qt::LeftButton, Qt::NoModifier, parent.rect().bottomRight());

        QTest::mouseClick(tabButtons[PropTabBar::TrackersTab], Qt::LeftButton);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::TrackersTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);
        QVERIFY(tabButtons[PropTabBar::TrackersTab]->isChecked());
    }

    void testKeyboardInteraction() const
    {
        QWidget parent;
        PropTabBar tabBar(&parent);
        show(parent);

        const QList<QPushButton *> tabButtons = buttons(tabBar);
        QPushButton *mainButton = tabButtons[PropTabBar::MainTab];
        QSignalSpy tabChangedSpy(&tabBar, &PropTabBar::tabChanged);
        QSignalSpy visibilitySpy(&tabBar, &PropTabBar::visibilityToggled);

        mainButton->setFocus();
        QTest::keyClick(mainButton, Qt::Key_Space);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::MainTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);

        QTest::keyClick(mainButton, Qt::Key_Space);
        QCOMPARE(tabBar.currentIndex(), -1);
        QCOMPARE(checkedButtonCount(tabButtons), 0);

        QTest::keyClick(mainButton, Qt::Key_Space);
        QCOMPARE(tabBar.currentIndex(), PropTabBar::MainTab);
        QCOMPARE(checkedButtonCount(tabButtons), 1);

        QCOMPARE(tabChangedSpy.count(), 2);
        QCOMPARE(visibilitySpy.count(), 3);
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

        QAccessibleInterface *interface = QAccessible::queryAccessibleInterface(tabButtons[PropTabBar::PeersTab]);
        QVERIFY(interface);
        QAccessibleActionInterface *actionInterface = interface->actionInterface();
        QVERIFY(actionInterface);
        actionInterface->doAction(QAccessibleActionInterface::toggleAction());

        QCOMPARE(tabBar.currentIndex(), -1);
        QCOMPARE(checkedButtonCount(tabButtons), 0);
        QCOMPARE(tabChangedSpy.count(), 2);
        QCOMPARE(visibilitySpy.count(), 1);
        QCOMPARE(visibilitySpy.at(0).at(0).toBool(), false);
    }
};

QTEST_MAIN(TestPropTabBar)
#include "testproptabbar.moc"
