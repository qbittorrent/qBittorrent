/****************************************************************************
** 
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
** All rights reserved.
** Contact: Nokia Corporation (qt-info@nokia.com)
** 
** This file is part of a Qt Solutions component.
**
** Commercial Usage  
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Solutions Commercial License Agreement provided
** with the Software or, alternatively, in accordance with the terms
** contained in a written agreement between you and Nokia.
** 
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
** 
** In addition, as a special exception, Nokia gives you certain
** additional rights. These rights are described in the Nokia Qt LGPL
** Exception version 1.1, included in the file LGPL_EXCEPTION.txt in this
** package.
** 
** GNU General Public License Usage 
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
** 
** Please note Third Party Software included with Qt Solutions may impose
** additional restrictions and it is the user's responsibility to ensure
** that they have met the licensing requirements of the GPL, LGPL, or Qt
** Solutions Commercial license and the relevant license of the Third
** Party Software they are using.
** 
** If you are unsure which license is appropriate for your use, please
** contact Nokia at qt-info@nokia.com.
** 
****************************************************************************/


#include "qtsinglecoreapplication.h"
#include "qtlocalpeer.h"

/*!
    \class QtSingleCoreApplication qtsinglecoreapplication.h
    \brief A variant of the QtSingleApplication class for non-GUI applications.

    This class is a variant of QtSingleApplication suited for use in
    console (non-GUI) applications. It is an extension of
    QCoreApplication (instead of QApplication). It does not require
    the QtGui library.

    The API and usage is identical to QtSingleApplication, except that
    functions relating to the "activation window" are not present, for
    obvious reasons. Please refer to the QtSingleApplication
    documentation for explanation of the usage.

    A QtSingleCoreApplication instance can communicate to a
    QtSingleApplication instance if they share the same application
    id. Hence, this class can be used to create a light-weight
    command-line tool that sends commands to a GUI application.

    \sa QtSingleApplication
*/

/*!
    Creates a QtSingleCoreApplication object. The application identifier
    will be QCoreApplication::applicationFilePath(). \a argc and \a
    argv are passed on to the QCoreAppliation constructor.
*/

QtSingleCoreApplication::QtSingleCoreApplication(int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    peer = new QtLocalPeer(this);
    connect(peer, SIGNAL(messageReceived(const QString&)), SIGNAL(messageReceived(const QString&)));
}


/*!
    Creates a QtSingleCoreApplication object with the application
    identifier \a appId. \a argc and \a argv are passed on to the
    QCoreAppliation constructor.
*/
QtSingleCoreApplication::QtSingleCoreApplication(const QString &appId, int &argc, char **argv)
    : QCoreApplication(argc, argv)
{
    peer = new QtLocalPeer(this, appId);
    connect(peer, SIGNAL(messageReceived(const QString&)), SIGNAL(messageReceived(const QString&)));
}


/*!
    Returns true if another instance of this application is running;
    otherwise false.

    This function does not find instances of this application that are
    being run by a different user (on Windows: that are running in
    another session).

    \sa sendMessage()
*/

bool QtSingleCoreApplication::isRunning()
{
    return peer->isClient();
}


/*!
    Tries to send the text \a message to the currently running
    instance. The QtSingleCoreApplication object in the running instance
    will emit the messageReceived() signal when it receives the
    message.

    This function returns true if the message has been sent to, and
    processed by, the current instance. If there is no instance
    currently running, or if the running instance fails to process the
    message within \a timeout milliseconds, this function return false.

    \sa isRunning(), messageReceived()
*/

bool QtSingleCoreApplication::sendMessage(const QString &message, int timeout)
{
    return peer->sendMessage(message, timeout);
}


/*!
    Returns the application identifier. Two processes with the same
    identifier will be regarded as instances of the same application.
*/

QString QtSingleCoreApplication::id() const
{
    return peer->applicationId();
}


/*!
    \fn void QtSingleCoreApplication::messageReceived(const QString& message)

    This signal is emitted when the current instance receives a \a
    message from another instance of this application.

    \sa sendMessage()
*/
