/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2024  Vladimir Golovnev <glassez@yandex.ru>
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

#pragma once

#include <QComboBox>
#include <QLineEdit>
#include <QtContainerFwd>
#include <QValidator>

#include "base/pathfwd.h"

class QAction;
class QContextMenuEvent;
class QFileIconProvider;
class QFileSystemModel;
class QKeyEvent;

namespace Private
{
    class FileSystemPathValidator final : public QValidator
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(FileSystemPathValidator)

    public:
        enum class TestResult
        {
            OK,
            DoesNotExist,
            NotADir,
            NotAFile,
            CantRead,
            CantWrite
        };

        FileSystemPathValidator(QObject *parent = nullptr);

        bool strictMode() const;
        void setStrictMode(bool value);

        bool existingOnly() const;
        void setExistingOnly(bool value);

        bool filesOnly() const;
        void setFilesOnly(bool value);

        bool directoriesOnly() const;
        void setDirectoriesOnly(bool value);

        bool checkReadPermission() const;
        void setCheckReadPermission(bool value);

        bool checkWritePermission() const;
        void setCheckWritePermission(bool value);

        TestResult lastTestResult() const;
        QValidator::State lastValidationState() const;

        QValidator::State validate(QString &input, int &pos) const override;

    private:
        TestResult testPath(const Path &path) const;

        bool m_strictMode = false;
        bool m_existingOnly = false;
        bool m_filesOnly = false;
        bool m_directoriesOnly = false;
        bool m_checkReadPermission = false;
        bool m_checkWritePermission = false;

        mutable TestResult m_lastTestResult = TestResult::DoesNotExist;
        mutable QValidator::State m_lastValidationState = QValidator::Invalid;
    };

    class IFileEditorWithCompletion
    {
    public:
        virtual ~IFileEditorWithCompletion() = default;
        virtual void completeDirectoriesOnly(bool completeDirsOnly) = 0;
        virtual void setFilenameFilters(const QStringList &filters) = 0;
        virtual void setBrowseAction(QAction *action) = 0;
        virtual void setValidator(QValidator *validator) = 0;
        virtual Path placeholder() const = 0;
        virtual void setPlaceholder(const Path &val) = 0;
        virtual QWidget *widget() = 0;
    };

    class FileLineEdit final : public QLineEdit, public IFileEditorWithCompletion
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(FileLineEdit)

    public:
        FileLineEdit(QWidget *parent = nullptr);
        ~FileLineEdit();

        void completeDirectoriesOnly(bool completeDirsOnly) override;
        void setFilenameFilters(const QStringList &filters) override;
        void setBrowseAction(QAction *action) override;
        void setValidator(QValidator *validator) override;
        Path placeholder() const override;
        void setPlaceholder(const Path &val) override;
        QWidget *widget() override;

    protected:
        void keyPressEvent(QKeyEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;

    private:
        void showCompletionPopup();
        void validateText();

        static QString warningText(FileSystemPathValidator::TestResult result);

        QFileSystemModel *m_completerModel = nullptr;
        QAction *m_browseAction = nullptr;
        QAction *m_warningAction = nullptr;
        QFileIconProvider *m_iconProvider = nullptr;
        bool m_completeDirectoriesOnly = false;
        QStringList m_filenameFilters;
    };

    class FileComboEdit final : public QComboBox, public IFileEditorWithCompletion
    {
        Q_OBJECT
        Q_DISABLE_COPY_MOVE(FileComboEdit)

    public:
        FileComboEdit(QWidget *parent = nullptr);

        void completeDirectoriesOnly(bool completeDirsOnly) override;
        void setFilenameFilters(const QStringList &filters) override;
        void setBrowseAction(QAction *action) override;
        void setValidator(QValidator *validator) override;
        Path placeholder() const override;
        void setPlaceholder(const Path &val) override;
        QWidget *widget() override;

    protected:
        QString text() const;
    };
}
