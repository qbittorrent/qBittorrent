/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2016 Eugene Shalygin
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

#include "fspathedit_p.h"

#include <QCompleter>
#include <QFileInfo>

Private::FileLineEdit::FileLineEdit(QWidget *parent)
    : QLineEdit {parent}
    , m_completerModel {new QFileSystemModel(this)}
    , m_completer {new QCompleter(this)}
    , m_browseAction {nullptr}
{
    m_completerModel->setRootPath("");
    m_completerModel->setIconProvider(&m_iconProvider);
    m_completer->setModel(m_completerModel);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(m_completer);

    connect(m_completerModel, &QFileSystemModel::directoryLoaded, this, &FileLineEdit::showCompletionPopup);
}

Private::FileLineEdit::~FileLineEdit()
{
    delete m_completerModel; // has to be deleted before deleting the m_iconProvider object
}

void Private::FileLineEdit::completeDirectoriesOnly(bool completeDirsOnly)
{
    QDir::Filters filters = completeDirsOnly ? QDir::Dirs : QDir::AllEntries;
    filters |= QDir::NoDotAndDotDot;
    m_completerModel->setFilter(filters);
}

void Private::FileLineEdit::setFilenameFilters(const QStringList &filters)
{
    m_completerModel->setNameFilters(filters);
}

void Private::FileLineEdit::setBrowseAction(QAction *action)
{
    m_browseAction = action;
}

QWidget *Private::FileLineEdit::widget()
{
    return this;
}

void Private::FileLineEdit::keyPressEvent(QKeyEvent *e)
{
    QLineEdit::keyPressEvent(e);
    if ((e->key() == Qt::Key_Space) && (e->modifiers() == Qt::CTRL)) {
        m_completerModel->setRootPath(QFileInfo(text()).absoluteDir().absolutePath());
        showCompletionPopup();
    }
}

void Private::FileLineEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    menu->addSeparator();
    if (m_browseAction) {
        menu->addSeparator();
        menu->addAction(m_browseAction);
    }
    menu->exec(event->globalPos());
    delete menu;
}

void Private::FileLineEdit::showCompletionPopup()
{
    m_completer->setCompletionPrefix(text());
    m_completer->complete();
}

Private::FileComboEdit::FileComboEdit(QWidget *parent)
    : QComboBox {parent}
{
    setEditable(true);
    setLineEdit(new FileLineEdit(this));
}

void Private::FileComboEdit::completeDirectoriesOnly(bool completeDirsOnly)
{
    static_cast<FileLineEdit *>(lineEdit())->completeDirectoriesOnly(completeDirsOnly);
}

void Private::FileComboEdit::setBrowseAction(QAction *action)
{
    static_cast<FileLineEdit *>(lineEdit())->setBrowseAction(action);
}

void Private::FileComboEdit::setFilenameFilters(const QStringList &filters)
{
    static_cast<FileLineEdit *>(lineEdit())->setFilenameFilters(filters);
}

QWidget *Private::FileComboEdit::widget()
{
    return this;
}

QString Private::FileComboEdit::text() const
{
    return currentText();
}
