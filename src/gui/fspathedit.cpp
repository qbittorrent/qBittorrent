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

#include "fspathedit.h"

#include <memory>
#include <stdexcept>

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QStyle>
#include <QToolButton>

#include "base/utils/fs.h"
#include "fspathedit_p.h"

namespace
{
    struct TrStringWithComment
    {
        const char *source;
        const char *comment;

        QString tr() const
        {
            return QCoreApplication::translate("FileSystemPathEdit", source, comment);
        }
    };

    constexpr TrStringWithComment browseButtonBriefText =
        QT_TRANSLATE_NOOP3("FileSystemPathEdit", "...", "Launch file dialog button text (brief)");
    constexpr TrStringWithComment browseButtonFullText =
        QT_TRANSLATE_NOOP3("FileSystemPathEdit", "&Browse...", "Launch file dialog button text (full)");
    constexpr TrStringWithComment defaultDialogCaptionForFile =
        QT_TRANSLATE_NOOP3("FileSystemPathEdit", "Choose a file", "Caption for file open/save dialog");
    constexpr TrStringWithComment defaultDialogCaptionForDirectory =
        QT_TRANSLATE_NOOP3("FileSystemPathEdit", "Choose a folder", "Caption for directory open dialog");
}

class FileSystemPathEdit::FileSystemPathEditPrivate
{
    Q_DECLARE_PUBLIC(FileSystemPathEdit)
    Q_DISABLE_COPY(FileSystemPathEditPrivate)

    FileSystemPathEditPrivate(FileSystemPathEdit *q, Private::FileEditorWithCompletion *editor);

    void modeChanged();
    void browseActionTriggered();
    QString dialogCaptionOrDefault() const;

    FileSystemPathEdit *q_ptr;
    std::unique_ptr<Private::FileEditorWithCompletion> m_editor;
    QAction *m_browseAction;
    QToolButton *m_browseBtn;
    QString m_fileNameFilter;
    Mode m_mode;
    QString m_lastSignaledPath;
    QString m_dialogCaption;
    Private::FileSystemPathValidator *m_validator;
};

FileSystemPathEdit::FileSystemPathEditPrivate::FileSystemPathEditPrivate(
                        FileSystemPathEdit *q, Private::FileEditorWithCompletion *editor)
    : q_ptr {q}
    , m_editor {editor}
    , m_browseAction {new QAction(q)}
    , m_browseBtn {new QToolButton(q)}
    , m_mode {FileSystemPathEdit::Mode::FileOpen}
    , m_validator {new Private::FileSystemPathValidator(q)}
{
    m_browseAction->setIconText(browseButtonBriefText.tr());
    m_browseAction->setText(browseButtonFullText.tr());
    m_browseAction->setToolTip(browseButtonFullText.tr().remove(QLatin1Char('&')));
    m_browseAction->setShortcut(Qt::CTRL + Qt::Key_B);
    m_browseBtn->setDefaultAction(m_browseAction);
    m_fileNameFilter = tr("Any file") + QLatin1String(" (*)");
    m_editor->setBrowseAction(m_browseAction);
    m_validator->setStrictMode(false);
    m_editor->setValidator(m_validator);
    modeChanged();
}

void FileSystemPathEdit::FileSystemPathEditPrivate::browseActionTriggered()
{
    Q_Q(FileSystemPathEdit);
    QString filter = q->fileNameFilter();
    QString directory = q->currentDirectory().isEmpty() ? QDir::homePath() : q->currentDirectory();

    QString selectedPath;
    switch (m_mode)
    {
    case FileSystemPathEdit::Mode::FileOpen:
        selectedPath = QFileDialog::getOpenFileName(q, dialogCaptionOrDefault(), directory, filter);
        break;
    case FileSystemPathEdit::Mode::FileSave:
        selectedPath = QFileDialog::getSaveFileName(q, dialogCaptionOrDefault(), directory, filter, &filter);
        break;
    case FileSystemPathEdit::Mode::DirectoryOpen:
    case FileSystemPathEdit::Mode::DirectorySave:
        selectedPath = QFileDialog::getExistingDirectory(q, dialogCaptionOrDefault(),
                                directory, QFileDialog::DontResolveSymlinks | QFileDialog::ShowDirsOnly);
        break;
    default:
        throw std::logic_error("Unknown FileSystemPathEdit mode");
    }
    if (!selectedPath.isEmpty())
        q->setEditWidgetText(Utils::Fs::toNativePath(selectedPath));
}

QString FileSystemPathEdit::FileSystemPathEditPrivate::dialogCaptionOrDefault() const
{
    if (!m_dialogCaption.isEmpty())
        return m_dialogCaption;

    switch (m_mode)
    {
    case FileSystemPathEdit::Mode::FileOpen:
    case FileSystemPathEdit::Mode::FileSave:
        return defaultDialogCaptionForFile.tr();
    case FileSystemPathEdit::Mode::DirectoryOpen:
    case FileSystemPathEdit::Mode::DirectorySave:
        return defaultDialogCaptionForDirectory.tr();
    default:
        throw std::logic_error("Unknown FileSystemPathEdit mode");
    }
}

void FileSystemPathEdit::FileSystemPathEditPrivate::modeChanged()
{
    QStyle::StandardPixmap pixmap = QStyle::SP_DialogOpenButton;
    bool showDirsOnly = false;
    switch (m_mode)
    {
    case FileSystemPathEdit::Mode::FileOpen:
    case FileSystemPathEdit::Mode::FileSave:
#ifdef Q_OS_WIN
        pixmap = QStyle::SP_DirOpenIcon;
#endif
        showDirsOnly = false;
        break;
    case FileSystemPathEdit::Mode::DirectoryOpen:
    case FileSystemPathEdit::Mode::DirectorySave:
        pixmap = QStyle::SP_DirOpenIcon;
        showDirsOnly = true;
        break;
    default:
        throw std::logic_error("Unknown FileSystemPathEdit mode");
    }
    m_browseAction->setIcon(QApplication::style()->standardIcon(pixmap));
    m_editor->completeDirectoriesOnly(showDirsOnly);

    m_validator->setExistingOnly(m_mode != FileSystemPathEdit::Mode::FileSave);
    m_validator->setDirectoriesOnly((m_mode == FileSystemPathEdit::Mode::DirectoryOpen) || (m_mode == FileSystemPathEdit::Mode::DirectorySave));
    m_validator->setCheckReadPermission((m_mode == FileSystemPathEdit::Mode::FileOpen) || (m_mode == FileSystemPathEdit::Mode::DirectoryOpen));
    m_validator->setCheckWritePermission((m_mode == FileSystemPathEdit::Mode::FileSave) || (m_mode == FileSystemPathEdit::Mode::DirectorySave));
}

FileSystemPathEdit::FileSystemPathEdit(Private::FileEditorWithCompletion *editor, QWidget *parent)
    : QWidget(parent)
    , d_ptr(new FileSystemPathEditPrivate(this, editor))
{
    Q_D(FileSystemPathEdit);
    editor->widget()->setParent(this);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(editor->widget());
    layout->addWidget(d->m_browseBtn);

    connect(d->m_browseAction, &QAction::triggered, this, [this]() { this->d_func()->browseActionTriggered(); });
}

FileSystemPathEdit::~FileSystemPathEdit()
{
    delete d_ptr;
}

QString FileSystemPathEdit::selectedPath() const
{
    return Utils::Fs::toUniformPath(editWidgetText());
}

void FileSystemPathEdit::setSelectedPath(const QString &val)
{
    Q_D(FileSystemPathEdit);
    setEditWidgetText(Utils::Fs::toNativePath(val));
    d->m_editor->widget()->setToolTip(val);
}

QString FileSystemPathEdit::fileNameFilter() const
{
    Q_D(const FileSystemPathEdit);
    return d->m_fileNameFilter;
}

void FileSystemPathEdit::setFileNameFilter(const QString &val)
{
    Q_D(FileSystemPathEdit);
    d->m_fileNameFilter = val;

#if 0
    // QFileSystemModel applies name filters to directories too.
    // To use the filters we have to subclass QFileSystemModel and skip directories while filtering
    // extract file masks
    const int openBracePos = val.indexOf(QLatin1Char('('), 0);
    const int closeBracePos = val.indexOf(QLatin1Char(')'), openBracePos + 1);
    if ((openBracePos > 0) && (closeBracePos > 0) && (closeBracePos > openBracePos + 2))
    {
        QString filterString = val.mid(openBracePos + 1, closeBracePos - openBracePos - 1);
        if (filterString == QLatin1String("*"))
        {        // no filters
            d->m_editor->setFilenameFilters({});
        }
        else
        {
            QStringList filters = filterString.split(QLatin1Char(' '), QString::SkipEmptyParts);
            d->m_editor->setFilenameFilters(filters);
        }
    }
    else
    {
        d->m_editor->setFilenameFilters({});
    }
#endif
}

bool FileSystemPathEdit::briefBrowseButtonCaption() const
{
    Q_D(const FileSystemPathEdit);
    return d->m_browseBtn->text() == browseButtonBriefText.tr();
}

void FileSystemPathEdit::setBriefBrowseButtonCaption(bool brief)
{
    Q_D(FileSystemPathEdit);
    d->m_browseBtn->setText(brief ? browseButtonBriefText.tr() : browseButtonFullText.tr());
}

void FileSystemPathEdit::onPathEdited()
{
    Q_D(FileSystemPathEdit);
    QString newPath = selectedPath();
    if (newPath != d->m_lastSignaledPath)
    {
        emit selectedPathChanged(newPath);
        d->m_lastSignaledPath = newPath;
        d->m_editor->widget()->setToolTip(editWidgetText());
    }
}

FileSystemPathEdit::Mode FileSystemPathEdit::mode() const
{
    Q_D(const FileSystemPathEdit);
    return d->m_mode;
}

void FileSystemPathEdit::setMode(FileSystemPathEdit::Mode theMode)
{
    Q_D(FileSystemPathEdit);
    d->m_mode = theMode;
    d->modeChanged();
}

QString FileSystemPathEdit::dialogCaption() const
{
    Q_D(const FileSystemPathEdit);
    return d->m_dialogCaption;
}

void FileSystemPathEdit::setDialogCaption(const QString &caption)
{
    Q_D(FileSystemPathEdit);
    d->m_dialogCaption = caption;
}

QString FileSystemPathEdit::currentDirectory() const
{
    return QFileInfo(selectedPath()).absoluteDir().absolutePath();
}

QWidget *FileSystemPathEdit::editWidgetImpl() const
{
    Q_D(const FileSystemPathEdit);
    return d->m_editor->widget();
}

// ------------------------- FileSystemPathLineEdit ----------------------
FileSystemPathLineEdit::FileSystemPathLineEdit(QWidget *parent)
    : FileSystemPathEdit(new WidgetType(), parent)
{
    connect(editWidget<WidgetType>(), &QLineEdit::editingFinished, this, &FileSystemPathLineEdit::onPathEdited);
    connect(editWidget<WidgetType>(), &QLineEdit::textChanged, this, &FileSystemPathLineEdit::onPathEdited);
}

QString FileSystemPathLineEdit::editWidgetText() const
{
    return editWidget<WidgetType>()->text();
}

void FileSystemPathLineEdit::clear()
{
    editWidget<WidgetType>()->clear();
}

void FileSystemPathLineEdit::setEditWidgetText(const QString &text)
{
    editWidget<WidgetType>()->setText(text);
}

// ----------------------- FileSystemPathComboEdit -----------------------
FileSystemPathComboEdit::FileSystemPathComboEdit(QWidget *parent)
    : FileSystemPathEdit(new WidgetType(), parent)
{
    editWidget<WidgetType>()->setEditable(true);
    connect(editWidget<WidgetType>(), &QComboBox::currentTextChanged, this, &FileSystemPathComboEdit::onPathEdited);
    connect(editWidget<WidgetType>()->lineEdit(), &QLineEdit::editingFinished, this, &FileSystemPathComboEdit::onPathEdited);
}

void FileSystemPathComboEdit::clear()
{
    editWidget<WidgetType>()->clear();
}

int FileSystemPathComboEdit::count() const
{
    return editWidget<WidgetType>()->count();
}

QString FileSystemPathComboEdit::item(int index) const
{
    return Utils::Fs::toUniformPath(editWidget<WidgetType>()->itemText(index));
}

void FileSystemPathComboEdit::addItem(const QString &text)
{
    editWidget<WidgetType>()->addItem(Utils::Fs::toNativePath(text));
}

void FileSystemPathComboEdit::insertItem(int index, const QString &text)
{
    editWidget<WidgetType>()->insertItem(index, Utils::Fs::toNativePath(text));
}

int FileSystemPathComboEdit::currentIndex() const
{
    return editWidget<WidgetType>()->currentIndex();
}

void FileSystemPathComboEdit::setCurrentIndex(int index)
{
    editWidget<WidgetType>()->setCurrentIndex(index);
}

int FileSystemPathComboEdit::maxVisibleItems() const
{
    return editWidget<WidgetType>()->maxVisibleItems();
}

void FileSystemPathComboEdit::setMaxVisibleItems(int maxItems)
{
    editWidget<WidgetType>()->setMaxVisibleItems(maxItems);
}

QString FileSystemPathComboEdit::editWidgetText() const
{
    return editWidget<WidgetType>()->currentText();
}

void FileSystemPathComboEdit::setEditWidgetText(const QString &text)
{
    editWidget<WidgetType>()->setCurrentText(text);
}
