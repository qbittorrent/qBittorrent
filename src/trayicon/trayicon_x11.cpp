/*
 * trayicon_x11.cpp - X11 trayicon (for use with KDE and GNOME)
 * Copyright (C) 2003  Justin Karneges
 * GNOME2 Notification Area support: Tomasz Sterna
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

#include <QApplication>
#include <QDesktopWidget>
#include <QPixmap>
#include <QBitmap>
#include <QToolTip>
#include <QPainter>
#include <QX11Info>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>

//----------------------------------------------------------------------------
// common stuff
//----------------------------------------------------------------------------

// for Gnome2 Notification Area
static XErrorHandler old_handler = 0;
static int dock_xerror = 0;
extern "C" int dock_xerrhandler(Display* dpy, XErrorEvent* err)
{
	dock_xerror = err->error_code;
	return old_handler(dpy, err);
}

static void trap_errors()
{
	dock_xerror = 0;
	old_handler = XSetErrorHandler(dock_xerrhandler);
}

static bool untrap_errors()
{
	XSetErrorHandler(old_handler);
	return (dock_xerror == 0);
}

static bool send_message(
	Display* dpy,	/* display */
	Window w,	/* sender (tray icon window) */
	long message,	/* message opcode */
	long data1,	/* message data 1 */
	long data2,	/* message data 2 */
	long data3	/* message data 3 */
)
{
	XEvent ev;

	memset(&ev, 0, sizeof(ev));
	ev.xclient.type = ClientMessage;
	ev.xclient.window = w;
	ev.xclient.message_type = XInternAtom(dpy, "_NET_SYSTEM_TRAY_OPCODE", False);
	ev.xclient.format = 32;
	ev.xclient.data.l[0] = CurrentTime;
	ev.xclient.data.l[1] = message;
	ev.xclient.data.l[2] = data1;
	ev.xclient.data.l[3] = data2;
	ev.xclient.data.l[4] = data3;

	trap_errors();
	XSendEvent(dpy, w, False, NoEventMask, &ev);
	XSync(dpy, False);
	return untrap_errors();
}

#define SYSTEM_TRAY_REQUEST_DOCK    0
//#define SYSTEM_TRAY_BEGIN_MESSAGE   1
//#define SYSTEM_TRAY_CANCEL_MESSAGE  2

//----------------------------------------------------------------------------
// TrayIcon::TrayIconPrivate
//----------------------------------------------------------------------------

class TrayIcon::TrayIconPrivate : public QWidget
{
public:
	TrayIconPrivate(TrayIcon *object, int size);
	~TrayIconPrivate() { }

	virtual void initWM(WId icon);
	virtual void setPixmap(const QPixmap &pm);
	virtual void paintEvent(QPaintEvent *);
	virtual void enterEvent(QEvent *);
	virtual void mouseMoveEvent(QMouseEvent *e);
	virtual void mousePressEvent(QMouseEvent *e);
	virtual void mouseReleaseEvent(QMouseEvent *e);
	virtual void mouseDoubleClickEvent(QMouseEvent *e);
	virtual void closeEvent(QCloseEvent *e);

	void updateMask();

private:
	TrayIcon *iconObject;
	QPixmap pix;
	int size;
};

TrayIcon::TrayIconPrivate::TrayIconPrivate(TrayIcon *object, int _size)
	: QWidget(0/*, Qt::WRepaintNoErase*/)
{
	setObjectName(QCoreApplication::instance()->applicationName() + "dock");
	iconObject = object;
	size = _size;

	setFocusPolicy(Qt::NoFocus);

// 	setBackgroundMode(Qt::X11ParentRelative);
	setAttribute(Qt::WA_NoSystemBackground);
	setBackgroundRole(QPalette::NoRole);

	// This resize will not work as expected,
	// we will get resized inside the tray later on.
	resize(size, size);
        setMinimumSize(size, size);
        setMaximumSize(size, size);
}

// This base stuff is required by both FreeDesktop specification and WindowMaker
void TrayIcon::TrayIconPrivate::initWM(WId icon)
{
	Display *dsp = QX11Info::display();
	WId leader   = winId();
	char *resName = objectName().toLocal8Bit().data();
	char *resClass = QCoreApplication::instance()->applicationName().toLocal8Bit().data();

	// set the class hint
	XClassHint classhint;
	classhint.res_name  = resName;
	classhint.res_class = resClass;
	XSetClassHint(dsp, leader, &classhint);

	// set the Window Manager hints
	XWMHints *hints;
	hints = XGetWMHints(dsp, leader); // init hints
	hints->flags = WindowGroupHint | IconWindowHint | StateHint;
	hints->window_group = leader;
	hints->initial_state = WithdrawnState;
	hints->icon_window = icon; // in WM, this should be winId() of separate widget
	hints->icon_x = 0;
	hints->icon_y = 0;
	XSetWMHints(dsp, leader, hints); // set the window hints for WM to use.
	XFree(hints);
}

void TrayIcon::TrayIconPrivate::setPixmap(const QPixmap &pm)
{
  QBitmap mask( QWidget::size() );

  if( pm.size() != QWidget::size() ) {
		// let's make a new mask
    mask.fill(Qt::color0);
    QPainter maskPainter(&mask);

		// draw the old mask in the center
    maskPainter.drawPixmap((width()  - pm.width() ) / 2,
                            (height() - pm.height()) / 2, pm.mask());

    QPixmap newPix(QWidget::size());
    QPainter pixPainter( &newPix );

		// draw the old pixmap in the center
    pixPainter.drawPixmap((width() - pm.width())/2,
                           (height() - pm.height())/2, pm);
    newPix.setMask(mask);
    pix = newPix;
  } else {
    pix = pm;
    mask = pm.mask();
  }

  setMask(mask);
  setWindowIcon(pix);
  repaint();
}

void TrayIcon::TrayIconPrivate::updateMask()
{
	if (!pix.isNull())
	{
		QBitmap mask(width(), height());
		mask.clear();
		QPainter mp(&mask);
		mp.drawPixmap((width() - pix.width())/2, (height() - pix.height())/2, pix.mask());
		mp.end();
		setMask(mask);
	}
}

void TrayIcon::TrayIconPrivate::paintEvent(QPaintEvent *)
{
	QPainter p(this);
// 	p.drawPixmap((width() - pix.width())/2, (height() - pix.height())/2, pix);
        p.drawPixmap(0, 0, pix);
}

void TrayIcon::TrayIconPrivate::enterEvent(QEvent *e)
{
	// Taken from KSystemTray..
/*#if QT_VERSION < 0x030200
	if ( !qApp->focusWidget() )
	{*/
		XEvent ev;
		memset(&ev, 0, sizeof(ev));
		ev.xfocus.display = QX11Info::display();
		ev.xfocus.type = FocusIn;
		ev.xfocus.window = winId();
		ev.xfocus.mode = NotifyNormal;
		ev.xfocus.detail = NotifyAncestor;

// 		unsigned long oldTime = QX11Info::appTime();
// 		QX11Info::setAppTime(1);
		qApp->x11ProcessEvent( &ev );
// 		QX11Info::setAppTime(oldTime);

	/*}
#endif*/

	QWidget::enterEvent(e);
}

void TrayIcon::TrayIconPrivate::mouseMoveEvent(QMouseEvent *e)
{
	QApplication::sendEvent(iconObject, e);
}

void TrayIcon::TrayIconPrivate::mousePressEvent(QMouseEvent *e)
{
	QApplication::sendEvent(iconObject, e);
}

void TrayIcon::TrayIconPrivate::mouseReleaseEvent(QMouseEvent *e)
{
	QApplication::sendEvent(iconObject, e);
}

void TrayIcon::TrayIconPrivate::mouseDoubleClickEvent(QMouseEvent *e)
{
	QApplication::sendEvent(iconObject, e);
}

void TrayIcon::TrayIconPrivate::closeEvent(QCloseEvent *e)
{
	iconObject->gotCloseEvent();
	e->accept();
}

//----------------------------------------------------------------------------
// TrayIconFreeDesktop
//----------------------------------------------------------------------------

class TrayIconFreeDesktop : public TrayIcon::TrayIconPrivate
{
public:
	TrayIconFreeDesktop(TrayIcon *object, const QPixmap &pm);
protected:
	virtual bool x11Event(XEvent*);
};

TrayIconFreeDesktop::TrayIconFreeDesktop(TrayIcon *object, const QPixmap &pm)
	: TrayIconPrivate(object, 22)
{
	initWM( winId() );

	// initialize NetWM
	Display *dsp = QX11Info::display();

	// dock the widget (adapted from SIM-ICQ)
	Screen *screen = XDefaultScreenOfDisplay(dsp); // get the screen
	int screen_id = XScreenNumberOfScreen(screen); // and it's number

	// tell X that we want to see ClientMessage and Deleted events, which
	// are picked up by QApplication::x11EventFilter
	Window root_window = QApplication::desktop()->winId();
	XWindowAttributes attr;

	XGetWindowAttributes(dsp, root_window, &attr);
	XSelectInput(dsp, root_window, attr.your_event_mask | StructureNotifyMask);

	char buf[32];
	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen_id);
	Atom selection_atom = XInternAtom(dsp, buf, false);
	XGrabServer(dsp);
	Window manager_window = XGetSelectionOwner(dsp, selection_atom);
	if ( manager_window != None )
		XSelectInput(dsp, manager_window, StructureNotifyMask);
	XUngrabServer(dsp);
	XFlush(dsp);

	if ( manager_window != None )
	{
		send_message(dsp, manager_window, SYSTEM_TRAY_REQUEST_DOCK, winId(), 0, 0);
	}
	else
	{
		object->hide();
		return;
	}

	// some KDE mumbo-jumbo... why is it there? anybody?
	Atom kwm_dockwindow_atom = XInternAtom(dsp, "KWM_DOCKWINDOW", false);
	Atom kde_net_system_tray_window_for_atom = XInternAtom(dsp, "_KDE_NET_WM_SYSTEM_TRAY_WINDOW_FOR", false);

	long data = 0;
	XChangeProperty(dsp, winId(), kwm_dockwindow_atom, kwm_dockwindow_atom, 32, PropModeReplace, (uchar*)&data, 1);
	XChangeProperty(dsp, winId(), kde_net_system_tray_window_for_atom, XA_WINDOW, 32, PropModeReplace, (uchar*)&data, 1);

	setPixmap(pm);
}

bool TrayIconFreeDesktop::x11Event(XEvent *ev)
{
	switch(ev->type)
	{
	case ReparentNotify:
// 		setUpdatesEnabled(false);
		show();
// 		updateMask();
// 		setUpdatesEnabled(true);
	}
	return false;
}

//----------------------------------------------------------------------------
// TrayIconWindowMaker
//----------------------------------------------------------------------------

class TrayIconWharf : public TrayIcon::TrayIconPrivate
{
public:
	TrayIconWharf(TrayIcon *object, const QPixmap &pm)
		: TrayIconPrivate(object, 44)
	{
		char *resName = QString(objectName() + "-wharf").toLocal8Bit().data();
		char *resClass = QCoreApplication::instance()->applicationName().toLocal8Bit().data();

		// set the class hint
		XClassHint classhint;
		classhint.res_name  = resName;
		classhint.res_class = resClass;
		XSetClassHint(QX11Info::display(), winId(), &classhint);

		setPixmap(pm);
	}

	void setPixmap(const QPixmap &_pm){
          QPixmap pm;
          QImage i = _pm.toImage();
          i = i.scaled(i.width() * 2, i.height() * 2);
          pm.fromImage(i);

          TrayIconPrivate::setPixmap(pm);

	  // thanks to Robert Spier for this:
	  // for some reason the repaint() isn't being honored, or isn't for
	  // the icon.  So force one on the widget behind the icon
//           erase();
          QPaintEvent pe( rect() );
          paintEvent(&pe);
	}
};

class TrayIconWindowMaker : public TrayIcon::TrayIconPrivate
{
public:
	TrayIconWindowMaker(TrayIcon *object, const QPixmap &pm);
	~TrayIconWindowMaker();

	void setPixmap(const QPixmap &pm);

private:
	TrayIconWharf *wharf;
};

TrayIconWindowMaker::TrayIconWindowMaker(TrayIcon *object, const QPixmap &pm)
	: TrayIconPrivate(object, 32)
{
	wharf = new TrayIconWharf(object, pm);
	initWM( wharf->winId() );
}

TrayIconWindowMaker::~TrayIconWindowMaker()
{
	delete wharf;
}

void TrayIconWindowMaker::setPixmap(const QPixmap &pm)
{
	wharf->setPixmap(pm);
}

//----------------------------------------------------------------------------
// TrayIcon
//----------------------------------------------------------------------------

void TrayIcon::sysInstall()
{
	if ( d )
		return;
	if ( v_isWMDock )
		d = (TrayIconPrivate *)(new TrayIconWindowMaker(this, pm));
	else
		d = (TrayIconPrivate *)(new TrayIconFreeDesktop(this, pm));

	sysUpdateToolTip();

	if ( v_isWMDock )
		d->show();
}

void TrayIcon::sysRemove()
{
	if ( !d )
		return;
	delete d;
	d = 0;
}

void TrayIcon::sysUpdateIcon()
{
	if ( !d )
		return;
	QPixmap pix = pm;
	d->setPixmap(pix);
}

void TrayIcon::sysUpdateToolTip()
{
	if ( !d )
		return;
	if ( tip.isEmpty() )
		d->setToolTip(QString::null);
	else
		d->setToolTip(tip);
}
