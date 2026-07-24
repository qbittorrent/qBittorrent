/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2026  FTA7700
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

#include <QList>
#include <QPoint>
#include <QToolBar>

class QAction;
class QLabel;
class QMouseEvent;
class QTimer;
class QWidget;

class CustomizableToolBar final : public QToolBar
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(CustomizableToolBar)

public:
    explicit CustomizableToolBar(QWidget *parent = nullptr);
    explicit CustomizableToolBar(const QString &title, QWidget *parent = nullptr);

    void lockAction(QAction *action);
    void setLocked(bool locked);

signals:
    void actionOrderChanged();

private:
    void actionEvent(QActionEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

    bool handleMousePress(QWidget *widget, QMouseEvent *mouseEvent);
    bool handleMouseMove(QMouseEvent *mouseEvent);
    bool handleMouseRelease(QMouseEvent *mouseEvent);
    void onDragTimer();

    int findBoundaryIndex() const;
    int getBoundaryGlobalX() const;
    void startDrag(QWidget *widget, const QPoint &globalPos);
    void updateDrag(const QPoint &globalPos);
    void endDrag(const QPoint &globalPos);

    QList<QAction *> m_lockedActions;
    bool m_locked = true;

    QAction *m_dragAction = nullptr;
    QAction *m_gapTarget = nullptr;
    QWidget *m_dragWidget = nullptr;
    QLabel *m_floatLabel = nullptr;
    QTimer *m_dragTimer = nullptr;
    QPoint m_dragStartPos;
    int m_dragOffsetX = 0;
    int m_lastFloatCentreX = 0;
    int m_lastSwapX = -1;
    bool m_dragging = false;
    bool m_dragJustFinished = false;
};
