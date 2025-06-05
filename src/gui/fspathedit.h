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

#pragma once

#include <QWidget>

#include "base/path.h"

namespace Private
{
    class FileComboEdit;
    class FileLineEdit;
    class IFileEditorWithCompletion;
}

/*!
 * \brief
 * Widget for editing strings which are paths in filesystem
 */
class FileSystemPathEdit : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(FileSystemPathEdit)

    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    Q_PROPERTY(Path selectedPath READ selectedPath WRITE setSelectedPath NOTIFY selectedPathChanged)
    Q_PROPERTY(QString fileNameFilter READ fileNameFilter WRITE setFileNameFilter)
    Q_PROPERTY(QString dialogCaption READ dialogCaption WRITE setDialogCaption)

    class FileSystemPathEditPrivate;

public:
    ~FileSystemPathEdit() override;

    enum class Mode
    {
        FileOpen,        //!< opening files, shows open file dialog
        FileSave,        //!< saving files, shows save file dialog
        DirectoryOpen,   //!< selecting existing directories
        DirectorySave,   //!< selecting directories for saving
        ReadOnly         //!< no browse button and no dialog, only validate path and check read permission
    };
    Q_ENUM(Mode)

    Mode mode() const;
    void setMode(Mode mode);

    Path selectedPath() const;
    void setSelectedPath(const Path &val);

    QString fileNameFilter() const;
    void setFileNameFilter(const QString &val);

    Path placeholder() const;
    void setPlaceholder(const Path &val);

    /// The browse button caption is "..." if true, and "Browse" otherwise
    bool briefBrowseButtonCaption() const;
    void setBriefBrowseButtonCaption(bool brief);

    QString dialogCaption() const;
    void setDialogCaption(const QString &caption);

    virtual void clear() = 0;

signals:
    void selectedPathChanged(const Path &path);

protected:
    explicit FileSystemPathEdit(Private::IFileEditorWithCompletion *editor, QWidget *parent);

    template <class Widget>
    Widget *editWidget() const
    {
        return static_cast<Widget *>(editWidgetImpl());
    }

protected slots:
    void onPathEdited();

private:
    Q_DECLARE_PRIVATE(FileSystemPathEdit)

    virtual QString editWidgetText() const = 0;
    virtual void setEditWidgetText(const QString &text) = 0;

    QWidget *editWidgetImpl() const;

    FileSystemPathEditPrivate *d_ptr = nullptr;
};

/// Widget which uses QLineEdit for path editing
class FileSystemPathLineEdit final : public FileSystemPathEdit
{
    using base = FileSystemPathEdit;
    using WidgetType = Private::FileLineEdit;

public:
    explicit FileSystemPathLineEdit(QWidget *parent = nullptr);

    void clear() override;

private:
    QString editWidgetText() const override;
    void setEditWidgetText(const QString &text) override;
};

/// Widget which uses QComboBox for path editing
class FileSystemPathComboEdit final : public FileSystemPathEdit
{
    using base = FileSystemPathEdit;
    using WidgetType = Private::FileComboEdit;

public:
    explicit FileSystemPathComboEdit(QWidget *parent = nullptr);

    void clear() override;

    int count() const;
    Path item(int index) const;
    void addItem(const Path &path);
    void insertItem(int index, const Path &path);

    int currentIndex() const;
    void setCurrentIndex(int index);

    int maxVisibleItems() const;
    void setMaxVisibleItems(int maxItems);

private:
    QString editWidgetText() const override;
    void setEditWidgetText(const QString &text) override;
};
