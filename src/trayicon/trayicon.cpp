/*
 * trayicon.cpp - system-independent trayicon class (adapted from Qt example)
 * Copyright (C) 2003  Justin Karneges
 * Qt4 port: Stefan Gehn
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include "trayicon.h"

/*!
  \class TrayIcon qtrayicon.h
  \brief The TrayIcon class implements an entry in the system tray.
*/

/*!
  Creates a TrayIcon object. \a parent and \a name are propagated
  to the QObject constructor. The icon is initially invisible.

  \sa show
*/
TrayIcon::TrayIcon( QObject *parent)
: QObject(parent), pop(0), d(0)
{
	v_isWMDock = FALSE;
}

/*!
  Creates a TrayIcon object displaying \a icon and \a tooltip, and opening
  \a popup when clicked with the right mousebutton. \a parent and \a name are
  propagated to the QObject constructor. The icon is initially invisible.

  \sa show
*/
TrayIcon::TrayIcon( const QPixmap &icon, const QString &tooltip, QMenu *popup, QObject *parent )
: QObject(parent), pop(popup), pm(icon), tip(tooltip), d(0)
{
	v_isWMDock = FALSE;

	if ( !pm.width() || !pm.height() )
		pm = QPixmap( 16, 16 );
}

/*!
  Removes the icon from the system tray and frees all allocated resources.
*/
TrayIcon::~TrayIcon()
{
    sysRemove();
}

/*!
  Informs the trayicon that the notification owner has probably been changed;
  and that it should attempt to register or re-register.
*/
void TrayIcon::newTrayOwner()
{
    // detach ourself from any existing notification area.
    hide();
    // show ourself on the new notification area
    show();
}


/*!
  Sets the context menu to \a popup. The context menu will pop up when the
  user clicks the system tray entry with the right mouse button.
*/
void TrayIcon::setPopup( QMenu* popup )
{
    pop = popup;
}

/*!
  Returns the current popup menu.

  \sa setPopup
*/
QMenu* TrayIcon::popup() const
{
    return pop;
}

/*!
  \property TrayIcon::icon
  \brief the system tray icon.
*/
void TrayIcon::setIcon( const QPixmap &icon )
{
    //if(!popup()) {
    //    tip = "";
    //}

    pm = icon;
    sysUpdateIcon();
}

QPixmap TrayIcon::icon() const
{
    return pm;
}

/*!
  \property TrayIcon::toolTip
  \brief the tooltip for the system tray entry

  On some systems, the tooltip's length is limited and will be truncated as necessary.
*/
void TrayIcon::setToolTip( const QString &tooltip )
{
    tip = tooltip;
    sysUpdateToolTip();
}

QString TrayIcon::toolTip() const
{
    return tip;
}

/*!
  Shows the icon in the system tray.

  \sa hide
*/
void TrayIcon::show()
{
    sysInstall();
}

/*!
  Hides the system tray entry.
*/
void TrayIcon::hide()
{
    sysRemove();
}

/*!
  \reimp
*/
bool TrayIcon::event( QEvent *e )
{
    switch ( e->type() ) {
    case QEvent::MouseMove:
	mouseMoveEvent( (QMouseEvent*)e );
	break;

    case QEvent::MouseButtonPress:
	mousePressEvent( (QMouseEvent*)e );
	break;

    case QEvent::MouseButtonRelease:
	mouseReleaseEvent( (QMouseEvent*)e );
	break;

    case QEvent::MouseButtonDblClick:
	mouseDoubleClickEvent( (QMouseEvent*)e );
	break;
    default:
	return QObject::event( e );
    }

    return TRUE;
}

/*!
  This event handler can be reimplemented in a subclass to receive
  mouse move events for the system tray entry.

  \sa mousePressEvent(), mouseReleaseEvent(), mouseDoubleClickEvent(),  QMouseEvent
*/
void TrayIcon::mouseMoveEvent( QMouseEvent *e )
{
    e->ignore();
}

/*!
  This event handler can be reimplemented in a subclass to receive
  mouse press events for the system tray entry.

  \sa mouseReleaseEvent(), mouseDoubleClickEvent(),
  mouseMoveEvent(), QMouseEvent
*/
void TrayIcon::mousePressEvent( QMouseEvent *e )
{
#ifndef Q_WS_WIN
// This is for X11, menus appear on mouse press
// I'm not sure whether Mac should be here or below.. Somebody check?
	switch ( e->button() ) {
		case Qt::RightButton:
			if ( pop ) {
				pop->popup( e->globalPos() );
				e->accept();
			}
			break;
		case Qt::LeftButton:
		case Qt::MidButton:
			emit clicked( e->globalPos(), e->button() );
			break;
		default:
			break;
	}
#endif
	e->ignore();
}

/*!
  This event handler can be reimplemented in a subclass to receive
  mouse release events for the system tray entry.

  The default implementations opens the context menu when the entry
  has been clicked with the right mouse button.

  \sa setPopup(), mousePressEvent(), mouseDoubleClickEvent(),
  mouseMoveEvent(), QMouseEvent
*/
void TrayIcon::mouseReleaseEvent( QMouseEvent *e )
{
#ifdef Q_WS_WIN
// This is for Windows, where menus appear on mouse release
	switch ( e->button() ) {
		case Qt::RightButton:
			if ( pop ) {
				// Necessary to make keyboard focus
				// and menu closing work on Windows.
				pop->activateWindow();
				pop->popup( e->globalPos() );
				pop->activateWindow();
				e->accept();
			}
			break;
		case Qt::LeftButton:
		case Qt::MidButton:
			emit clicked( e->globalPos(), e->button() );
			break;
		default:
			break;
	}
#endif
	e->ignore();
}

/*!
  This event handler can be reimplemented in a subclass to receive
  mouse double click events for the system tray entry.

  Note that the system tray entry gets a mousePressEvent() and a
  mouseReleaseEvent() before the mouseDoubleClickEvent().

  \sa mousePressEvent(), mouseReleaseEvent(),
  mouseMoveEvent(), QMouseEvent
*/
void TrayIcon::mouseDoubleClickEvent( QMouseEvent *e )
{
	if ( e->button() == Qt::LeftButton )
		emit doubleClicked( e->globalPos() );
	e->accept();
}

/*!
  \fn void TrayIcon::clicked( const QPoint &p )

  This signal is emitted when the user clicks the system tray icon
  with the left mouse button, with \a p being the global mouse position
  at that moment.
*/

/*!
  \fn void TrayIcon::doubleClicked( const QPoint &p )

  This signal is emitted when the user double clicks the system tray
  icon with the left mouse button, with \a p being the global mouse position
  at that moment.
*/

void TrayIcon::gotCloseEvent()
{
	closed();
}
