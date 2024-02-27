/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2023  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (c) 2007  Trolltech ASA <info@trolltech.com>
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

#include "lineedit.h"

#include <chrono>

#include <QAction>
#include <QKeyEvent>
#include <QTimer>

#include "base/global.h"
#include "uithememanager.h"

using namespace std::chrono_literals;

namespace
{
    const std::chrono::milliseconds FILTER_INPUT_DELAY {400};
}

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
    , m_delayedTextChangedTimer {new QTimer(this)}
{
    auto *action = new QAction(UIThemeManager::instance()->getIcon(u"edit-find"_s), QString(), this);
    addAction(action, QLineEdit::LeadingPosition);

    setClearButtonEnabled(true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_delayedTextChangedTimer->setSingleShot(true);
    connect(m_delayedTextChangedTimer, &QTimer::timeout, this, [this]
    {
        emit textChanged(text());
    });
    connect(this, &QLineEdit::textChanged, this, [this]
    {
        m_delayedTextChangedTimer->start(FILTER_INPUT_DELAY);
    });
}

void LineEdit::keyPressEvent(QKeyEvent *event)
{
    if ((event->modifiers() == Qt::NoModifier) && (event->key() == Qt::Key_Escape))
    {
        clear();
    }

    QLineEdit::keyPressEvent(event);
}
