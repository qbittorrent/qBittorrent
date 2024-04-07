/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2022  Mike Tzou (Chocobo1)
 * Copyright (C) 2016  Eugene Shalygin
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
#include <QContextMenuEvent>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenu>
#include <QStringList>
#include <QStyle>

#include "base/path.h"

// -------------------- FileSystemPathValidator ----------------------------------------
Private::FileSystemPathValidator::FileSystemPathValidator(QObject *parent)
    : QValidator(parent)
{
}

bool Private::FileSystemPathValidator::strictMode() const
{
    return m_strictMode;
}

void Private::FileSystemPathValidator::setStrictMode(const bool value)
{
    m_strictMode = value;
}

bool Private::FileSystemPathValidator::existingOnly() const
{
    return m_existingOnly;
}

void Private::FileSystemPathValidator::setExistingOnly(const bool value)
{
    m_existingOnly = value;
}

bool Private::FileSystemPathValidator::filesOnly() const
{
    return m_filesOnly;
}

void Private::FileSystemPathValidator::setFilesOnly(const bool value)
{
    m_filesOnly = value;
}

bool Private::FileSystemPathValidator::directoriesOnly() const
{
    return m_directoriesOnly;
}

void Private::FileSystemPathValidator::setDirectoriesOnly(const bool value)
{
    m_directoriesOnly = value;
}

bool Private::FileSystemPathValidator::checkReadPermission() const
{
    return m_checkReadPermission;
}

void Private::FileSystemPathValidator::setCheckReadPermission(const bool value)
{
    m_checkReadPermission = value;
}

bool Private::FileSystemPathValidator::checkWritePermission() const
{
    return m_checkWritePermission;
}

void Private::FileSystemPathValidator::setCheckWritePermission(const bool value)
{
    m_checkWritePermission = value;
}

Private::FileSystemPathValidator::TestResult
Private::FileSystemPathValidator::testPath(const Path &path) const
{
    // `QFileInfo` will cache the query results and avoid excessive querying to filesystem
    const QFileInfo info {path.data()};

    if (!info.exists())
        return existingOnly() ? TestResult::DoesNotExist : TestResult::OK;

    if (filesOnly())
    {
        if (!info.isFile())
            return TestResult::NotAFile;
    }

    if (directoriesOnly())
    {
        if (!info.isDir())
            return TestResult::NotADir;
    }

    if (checkReadPermission() && !info.isReadable())
        return TestResult::CantRead;

    if (checkWritePermission() && !info.isWritable())
        return TestResult::CantWrite;

    return TestResult::OK;
}

Private::FileSystemPathValidator::TestResult Private::FileSystemPathValidator::lastTestResult() const
{
    return m_lastTestResult;
}

QValidator::State Private::FileSystemPathValidator::lastValidationState() const
{
    return m_lastValidationState;
}

QValidator::State Private::FileSystemPathValidator::validate(QString &input, [[maybe_unused]] int &pos) const
{
    // we ignore cursor position and validate the full path anyway

    m_lastTestResult = testPath(Path(input));
    m_lastValidationState = (m_lastTestResult == TestResult::OK)
        ? QValidator::Acceptable
        : (strictMode() ? QValidator::Invalid : QValidator::Intermediate);

    return m_lastValidationState;
}

Private::FileLineEdit::FileLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    setCompleter(new QCompleter(this));
    connect(this, &QLineEdit::textChanged, this, &FileLineEdit::validateText);
}

Private::FileLineEdit::~FileLineEdit()
{
    delete m_completerModel; // has to be deleted before deleting the m_iconProvider object
    delete m_iconProvider;
}

void Private::FileLineEdit::completeDirectoriesOnly(const bool completeDirsOnly)
{
    m_completeDirectoriesOnly = completeDirsOnly;
    if (m_completerModel)
    {
        const QDir::Filters filters = QDir::NoDotAndDotDot
                | (completeDirsOnly ? QDir::Dirs : QDir::AllEntries);
        m_completerModel->setFilter(filters);
    }
}

void Private::FileLineEdit::setFilenameFilters(const QStringList &filters)
{
    m_filenameFilters = filters;
    if (m_completerModel)
        m_completerModel->setNameFilters(m_filenameFilters);
}

void Private::FileLineEdit::setBrowseAction(QAction *action)
{
    m_browseAction = action;
}

void Private::FileLineEdit::setValidator(QValidator *validator)
{
    QLineEdit::setValidator(validator);
}

Path Private::FileLineEdit::placeholder() const
{
    return Path(placeholderText());
}

void Private::FileLineEdit::setPlaceholder(const Path &val)
{
    setPlaceholderText(val.toString());
}

QWidget *Private::FileLineEdit::widget()
{
    return this;
}

void Private::FileLineEdit::keyPressEvent(QKeyEvent *e)
{
    QLineEdit::keyPressEvent(e);

    if ((e->key() == Qt::Key_Space) && (e->modifiers() == Qt::CTRL))
    {
        if (!m_completerModel)
        {
            m_iconProvider = new QFileIconProvider;
            m_iconProvider->setOptions(QFileIconProvider::DontUseCustomDirectoryIcons);

            m_completerModel = new QFileSystemModel(this);
            m_completerModel->setIconProvider(m_iconProvider);
            m_completerModel->setOptions(QFileSystemModel::DontWatchForChanges);
            m_completerModel->setNameFilters(m_filenameFilters);
            const QDir::Filters filters = QDir::NoDotAndDotDot
                    | (m_completeDirectoriesOnly ? QDir::Dirs : QDir::AllEntries);
            m_completerModel->setFilter(filters);

            completer()->setModel(m_completerModel);
        }

        m_completerModel->setRootPath(Path(text()).data());
        showCompletionPopup();
    }
}

void Private::FileLineEdit::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu *menu = createStandardContextMenu();
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (m_browseAction)
    {
        menu->addSeparator();
        menu->addAction(m_browseAction);
    }

    menu->popup(event->globalPos());
}

void Private::FileLineEdit::showCompletionPopup()
{
    completer()->setCompletionPrefix(text());
    completer()->complete();
}

void Private::FileLineEdit::validateText()
{
    const auto *validator = qobject_cast<const FileSystemPathValidator *>(this->validator());
    if (!validator)
        return;

    const FileSystemPathValidator::TestResult lastTestResult = validator->lastTestResult();
    const QValidator::State lastState = validator->lastValidationState();

    if (lastTestResult == FileSystemPathValidator::TestResult::OK)
    {
        delete m_warningAction;
        m_warningAction = nullptr;
    }
    else
    {
        if (!m_warningAction)
        {
            m_warningAction = new QAction(this);
            addAction(m_warningAction, QLineEdit::TrailingPosition);
        }
    }

    if (m_warningAction)
    {
        if (lastState == QValidator::Invalid)
            m_warningAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
        else if (lastState == QValidator::Intermediate)
            m_warningAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxInformation));
        m_warningAction->setToolTip(warningText(lastTestResult));
    }
}

QString Private::FileLineEdit::warningText(const FileSystemPathValidator::TestResult result)
{
    using TestResult = FileSystemPathValidator::TestResult;
    switch (result)
    {
    case TestResult::DoesNotExist:
        return tr("Path does not exist");
    case TestResult::NotADir:
        return tr("Path does not point to a directory");
    case TestResult::NotAFile:
        return tr("Path does not point to a file");
    case TestResult::CantRead:
        return tr("Don't have read permission to path");
    case TestResult::CantWrite:
        return tr("Don't have write permission to path");
    default:
        break;
    }
    return {};
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

void Private::FileComboEdit::setValidator(QValidator *validator)
{
    lineEdit()->setValidator(validator);
}

Path Private::FileComboEdit::placeholder() const
{
    return Path(lineEdit()->placeholderText());
}

void Private::FileComboEdit::setPlaceholder(const Path &val)
{
    lineEdit()->setPlaceholderText(val.toString());
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
