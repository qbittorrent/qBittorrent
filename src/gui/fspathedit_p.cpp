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
#include <QContextMenuEvent>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QMenu>
#include <QStringList>
#include <QStyle>

// -------------------- FileSystemPathValidator ----------------------------------------
Private::FileSystemPathValidator::FileSystemPathValidator(QObject *parent)
    : QValidator(parent)
    , m_strictMode(false)
    , m_existingOnly(false)
    , m_directoriesOnly(false)
    , m_checkReadPermission(false)
    , m_checkWritePermission(false)
    , m_lastTestResult(TestResult::DoesNotExist)
    , m_lastValidationState(QValidator::Invalid)
{
}

bool Private::FileSystemPathValidator::strictMode() const
{
    return m_strictMode;
}

void Private::FileSystemPathValidator::setStrictMode(bool v)
{
    m_strictMode = v;
}

bool Private::FileSystemPathValidator::existingOnly() const
{
    return m_existingOnly;
}

void Private::FileSystemPathValidator::setExistingOnly(bool v)
{
    m_existingOnly = v;
}

bool Private::FileSystemPathValidator::directoriesOnly() const
{
    return m_directoriesOnly;
}

void Private::FileSystemPathValidator::setDirectoriesOnly(bool v)
{
    m_directoriesOnly = v;
}

bool Private::FileSystemPathValidator::checkReadPermission() const
{
    return m_checkReadPermission;
}

void Private::FileSystemPathValidator::setCheckReadPermission(bool v)
{
    m_checkReadPermission = v;
}

bool Private::FileSystemPathValidator::checkWritePermission() const
{
    return m_checkWritePermission;
}

void Private::FileSystemPathValidator::setCheckWritePermission(bool v)
{
    m_checkWritePermission = v;
}

QValidator::State Private::FileSystemPathValidator::validate(QString &input, int &pos) const
{
    if (input.isEmpty())
        return m_strictMode ? QValidator::Invalid : QValidator::Intermediate;

    // we test path components from beginning to the one with cursor location in strict mode
    // and the one with cursor and beyond in non-strict mode
    QList<QStringView> components = QStringView(input).split(QDir::separator(), Qt::KeepEmptyParts);
    // find index of the component that contains pos
    int componentWithCursorIndex = 0;
    int componentWithCursorPosition = 0;
    int pathLength = 0;

    // components.size() - 1 because when path ends with QDir::separator(), we will not see the last
    // character in the components array, yet everything past the one before the last delimiter
    // belongs to the last component
    for (; (componentWithCursorIndex < (components.size() - 1)) && (pathLength < pos); ++componentWithCursorIndex)
    {
        pathLength = componentWithCursorPosition + components[componentWithCursorIndex].size();
        componentWithCursorPosition += components[componentWithCursorIndex].size() + 1;
    }

    Q_ASSERT(componentWithCursorIndex < components.size());

    m_lastValidationState = QValidator::Acceptable;
    if (componentWithCursorIndex > 0)
        m_lastValidationState = validate(components, m_strictMode, 0, componentWithCursorIndex - 1);
    if ((m_lastValidationState == QValidator::Acceptable) && (componentWithCursorIndex < components.size()))
        m_lastValidationState = validate(components, false, componentWithCursorIndex, components.size() - 1);
    return m_lastValidationState;
}

QValidator::State Private::FileSystemPathValidator::validate(const QList<QStringView> &pathComponents, bool strict,
                                                             int firstComponentToTest, int lastComponentToTest) const
{
    Q_ASSERT(firstComponentToTest >= 0);
    Q_ASSERT(lastComponentToTest >= firstComponentToTest);
    Q_ASSERT(lastComponentToTest < pathComponents.size());

    m_lastTestResult = TestResult::DoesNotExist;
    if (pathComponents.empty())
        return strict ? QValidator::Invalid : QValidator::Intermediate;

    for (int i = firstComponentToTest; i <= lastComponentToTest; ++i)
    {
        const bool isFinalPath = (i == (pathComponents.size() - 1));
        const QStringView componentPath = pathComponents[i];
        if (componentPath.isEmpty()) continue;

        m_lastTestResult = testPath(pathComponents[i], isFinalPath);
        if (m_lastTestResult != TestResult::OK)
        {
            m_lastTestedPath = componentPath.toString();
            return strict ? QValidator::Invalid : QValidator::Intermediate;
        }
    }

    return QValidator::Acceptable;
}

Private::FileSystemPathValidator::TestResult
Private::FileSystemPathValidator::testPath(const QStringView path, bool pathIsComplete) const
{
    QFileInfo fi(path.toString());
    if (m_existingOnly && !fi.exists())
        return TestResult::DoesNotExist;

    if ((!pathIsComplete || m_directoriesOnly) && !fi.isDir())
        return TestResult::NotADir;

    if (pathIsComplete)
    {
        if (!m_directoriesOnly && fi.isDir())
            return TestResult::NotAFile;

        if (m_checkWritePermission && (fi.exists() && !fi.isWritable()))
            return TestResult::CantWrite;
        if (m_checkReadPermission && !fi.isReadable())
            return TestResult::CantRead;
    }

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

QString Private::FileSystemPathValidator::lastTestedPath() const
{
    return m_lastTestedPath;
}

Private::FileLineEdit::FileLineEdit(QWidget *parent)
    : QLineEdit {parent}
    , m_completerModel {new QFileSystemModel(this)}
    , m_completer {new QCompleter(this)}
    , m_browseAction {nullptr}
    , m_warningAction {nullptr}
{
    m_completerModel->setRootPath("");
    m_completerModel->setIconProvider(&m_iconProvider);
    m_completer->setModel(m_completerModel);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(m_completer);
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

void Private::FileLineEdit::setValidator(QValidator *validator)
{
    QLineEdit::setValidator(validator);
}

QString Private::FileLineEdit::placeholder() const
{
    return placeholderText();
}

void Private::FileLineEdit::setPlaceholder(const QString &val)
{
    setPlaceholderText(val);
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
        m_completerModel->setRootPath(QFileInfo(text()).absoluteDir().absolutePath());
        showCompletionPopup();
    }

    auto *validator = qobject_cast<const FileSystemPathValidator *>(this->validator());
    if (validator)
    {
        FileSystemPathValidator::TestResult lastTestResult = validator->lastTestResult();
        QValidator::State lastState = validator->lastValidationState();
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
                m_warningAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
            m_warningAction->setToolTip(warningText(lastTestResult).arg(validator->lastTestedPath()));
        }
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
    m_completer->setCompletionPrefix(text());
    m_completer->complete();
}

QString Private::FileLineEdit::warningText(FileSystemPathValidator::TestResult r)
{
    using TestResult = FileSystemPathValidator::TestResult;
    switch (r)
    {
    case TestResult::DoesNotExist:
        return tr("'%1' does not exist");
    case TestResult::NotADir:
        return tr("'%1' does not point to a directory");
    case TestResult::NotAFile:
        return tr("'%1' does not point to a file");
    case TestResult::CantRead:
        return tr("Does not have read permission in '%1'");
    case TestResult::CantWrite:
        return tr("Does not have write permission in '%1'");
    default:
        return {};
    }
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

QString Private::FileComboEdit::placeholder() const
{
    return lineEdit()->placeholderText();
}

void Private::FileComboEdit::setPlaceholder(const QString &val)
{
    lineEdit()->setPlaceholderText(val);
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
