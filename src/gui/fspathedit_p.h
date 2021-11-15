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

#include <QComboBox>
#include <QFileIconProvider>
#include <QLineEdit>
#include <QtContainerFwd>
#include <QValidator>

#include "base/pathfwd.h"

class QAction;
class QCompleter;
class QContextMenuEvent;
class QFileSystemModel;
class QKeyEvent;

namespace Private
{
    class FileSystemPathValidator final : public QValidator
    {
        Q_OBJECT

    public:
        FileSystemPathValidator(QObject *parent = nullptr);

        bool strictMode() const;
        void setStrictMode(bool v);

        bool existingOnly() const;
        void setExistingOnly(bool v);

        bool directoriesOnly() const;
        void setDirectoriesOnly(bool v);

        bool checkReadPermission() const;
        void setCheckReadPermission(bool v);

        bool checkWritePermission() const;
        void setCheckWritePermission(bool v);

        QValidator::State validate(QString &input, int &pos) const override;

        enum class TestResult
        {
            OK,
            DoesNotExist,
            NotADir,
            NotAFile,
            CantRead,
            CantWrite
        };

        TestResult lastTestResult() const;
        QValidator::State lastValidationState() const;
        QString lastTestedPath() const;

    private:
        QValidator::State validate(const QList<QStringView> &pathComponents, bool strict,
                                   int firstComponentToTest, int lastComponentToTest) const;

        TestResult testPath(const Path &path, bool pathIsComplete) const;

        bool m_strictMode;
        bool m_existingOnly;
        bool m_directoriesOnly;
        bool m_checkReadPermission;
        bool m_checkWritePermission;

        mutable TestResult m_lastTestResult;
        mutable QValidator::State m_lastValidationState;
        mutable QString m_lastTestedPath;
    };

    class FileEditorWithCompletion
    {
    public:
        virtual ~FileEditorWithCompletion() = default;
        virtual void completeDirectoriesOnly(bool completeDirsOnly) = 0;
        virtual void setFilenameFilters(const QStringList &filters) = 0;
        virtual void setBrowseAction(QAction *action) = 0;
        virtual void setValidator(QValidator *validator) = 0;
        virtual Path placeholder() const = 0;
        virtual void setPlaceholder(const Path &val) = 0;
        virtual QWidget *widget() = 0;
    };

    class FileLineEdit final : public QLineEdit, public FileEditorWithCompletion
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
        static QString warningText(FileSystemPathValidator::TestResult r);
        void showCompletionPopup();

        QFileSystemModel *m_completerModel;
        QCompleter *m_completer;
        QAction *m_browseAction;
        QFileIconProvider m_iconProvider;
        QAction *m_warningAction;
    };

    class FileComboEdit final : public QComboBox, public FileEditorWithCompletion
    {
        Q_OBJECT

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
