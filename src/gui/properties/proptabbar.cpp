/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2010  Christophe Dumez <chris@qbittorrent.org>
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

#include "proptabbar.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QKeySequence>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpacerItem>
#ifdef Q_OS_MACOS
#include <QStyleOptionButton>
#include <QStylePainter>
#endif

#include "base/global.h"
#include "gui/uithememanager.h"

namespace
{
#ifdef Q_OS_MACOS
    class PropTabButton final : public QPushButton
    {
    public:
        using QPushButton::QPushButton;

    private:
        void paintEvent(QPaintEvent *) override
        {
            QStyleOptionButton option;
            initStyleOption(&option);
            const bool isActive = option.state.testFlag(QStyle::State_Active);
            // State_On alone is only a subtle pressed shade on recent macOS versions;
            // the native default face supplies the user's accent and contrasting text.
            if (isChecked() && isActive)
                option.features |= QStyleOptionButton::DefaultButton;

            QStylePainter painter {this};
            painter.drawControl(QStyle::CE_PushButton, option);
            if (!isChecked() || isActive)
                return;

            const QRect contentsRect = style()->subElementRect(QStyle::SE_PushButtonContents, &option, this);
            const int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth, &option, this);
            const int verticalInset = contentsRect.top() - (2 * frameWidth);
            const QRect faceRect = option.rect.adjusted(0, verticalInset, 0, -verticalInset);
            const QPalette::ColorGroup colorGroup = isEnabled() ? QPalette::Inactive : QPalette::Disabled;

            painter.setRenderHint(QPainter::Antialiasing);
            painter.setPen(Qt::NoPen);
            painter.setBrush(option.palette.brush(colorGroup, QPalette::Highlight));
            painter.drawRoundedRect(faceRect, contentsRect.top(), contentsRect.top());

            option.palette.setBrush(colorGroup, QPalette::ButtonText
                    , option.palette.brush(colorGroup, QPalette::HighlightedText));
            painter.drawControl(QStyle::CE_PushButtonLabel, option);
        }
    };
#else
    using PropTabButton = QPushButton;
#endif
}

PropTabBar::PropTabBar(QWidget *parent)
    : QHBoxLayout(parent)
{
    setAlignment(Qt::AlignLeft | Qt::AlignCenter);
    setSpacing(3);
    m_btnGroup = new QButtonGroup(this);
    m_btnGroup->setExclusive(false);
    // General tab
    QPushButton *mainInfosButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"help-about"_s, u"document-properties"_s),
#endif
            tr("General"), parent);
    mainInfosButton->setShortcut(Qt::ALT | Qt::Key_G);
    addWidget(mainInfosButton);
    m_btnGroup->addButton(mainInfosButton, MainTab);
    // Trackers tab
    QPushButton *trackersButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"trackers"_s, u"network-server"_s),
#endif
            tr("Trackers"), parent);
    trackersButton->setShortcut(Qt::ALT | Qt::Key_C);
    addWidget(trackersButton);
    m_btnGroup->addButton(trackersButton, TrackersTab);
    // Peers tab
    QPushButton *peersButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"peers"_s),
#endif
            tr("Peers"), parent);
    peersButton->setShortcut(Qt::ALT | Qt::Key_R);
    addWidget(peersButton);
    m_btnGroup->addButton(peersButton, PeersTab);
    // URL seeds tab
    QPushButton *URLSeedsButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"network-server"_s),
#endif
            tr("HTTP Sources"), parent);
    URLSeedsButton->setShortcut(Qt::ALT | Qt::Key_B);
    addWidget(URLSeedsButton);
    m_btnGroup->addButton(URLSeedsButton, URLSeedsTab);
    // Files tab
    QPushButton *filesButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"directory"_s),
#endif
            tr("Content"), parent);
    filesButton->setShortcut(Qt::ALT | Qt::Key_Z);
    addWidget(filesButton);
    m_btnGroup->addButton(filesButton, FilesTab);
    // Spacer
    addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Fixed));
    // Speed tab
    QPushButton *speedButton = new PropTabButton(
#ifndef Q_OS_MACOS
            UIThemeManager::instance()->getIcon(u"chart-line"_s),
#endif
            tr("Speed"), parent);
    speedButton->setShortcut(Qt::ALT | Qt::Key_D);
    addWidget(speedButton);
    m_btnGroup->addButton(speedButton, SpeedTab);

    for (QAbstractButton *button : m_btnGroup->buttons())
    {
        button->setCheckable(true);
        button->setAutoExclusive(true);
    }

    // SIGNAL/SLOT
    connect(m_btnGroup, &QButtonGroup::idToggled, this, [this](const int index, const bool checked)
    {
        if (checked)
            setCurrentIndex(index);
        else if (m_currentIndex == index)
            setCurrentIndex(-1);
    });
}

int PropTabBar::currentIndex() const
{
    return m_currentIndex;
}

void PropTabBar::setCurrentIndex(int index)
{
    if (index >= m_btnGroup->buttons().size())
        index = 0;
    // If asked to hide or if the currently selected tab is clicked
    if ((index < 0) || (m_currentIndex == index))
    {
        if (m_currentIndex >= 0)
        {
            const QSignalBlocker blocker {m_btnGroup};
            m_btnGroup->button(m_currentIndex)->setChecked(false);
            m_currentIndex = -1;
            emit visibilityToggled(false);
        }
        return;
    }

    const bool wasHidden = (m_currentIndex < 0);
    const QSignalBlocker blocker {m_btnGroup};
    for (QAbstractButton *button : m_btnGroup->buttons())
    {
        if (m_btnGroup->id(button) != index)
            button->setChecked(false);
    }
    m_btnGroup->button(index)->setChecked(true);
    m_currentIndex = index;

    if (wasHidden)
    {
        // Nothing was selected, show!
        emit visibilityToggled(true);
    }

    // Emit the signal
    emit tabChanged(index);
}
