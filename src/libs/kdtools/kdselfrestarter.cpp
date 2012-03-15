/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#include "kdselfrestarter.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>

class KDSelfRestarter::Private
{
public:
    Private(int argc, char *argv[])
        : restartOnQuit(false)
    {
        executable = QString::fromLocal8Bit(argv[0]);
        workingPath = QDir::currentPath(); 
        for (int i = 1; i < argc; ++i)
            args << QString::fromLocal8Bit(argv[i]);
    }
   
    Private()
    {
        executable = qApp->applicationFilePath();
        workingPath = QDir::currentPath();
        args = qApp->arguments().mid(1);
    }

    ~Private()
    {
        if (restartOnQuit)
            QProcess::startDetached(executable, args, workingPath);
    }
    
    QString executable;
    QStringList args;
    bool restartOnQuit;
    QString workingPath;
    static KDSelfRestarter *instance;
};

KDSelfRestarter *KDSelfRestarter::Private::instance = 0;

KDSelfRestarter::KDSelfRestarter(int argc, char *argv[])
    : d(new Private(argc, argv))
{
    Q_ASSERT_X(!Private::instance, Q_FUNC_INFO, "Cannot create more than one KDSelfRestarter instance");
    Private::instance = this;
}

KDSelfRestarter::~KDSelfRestarter()
{
    Q_ASSERT_X(Private::instance == this, Q_FUNC_INFO, "Cannot create more than one KDSelfRestarter instance");
    delete d;
    Private::instance = 0;
}

void KDSelfRestarter::setRestartOnQuit(bool restart)
{
    Q_ASSERT_X(Private::instance, Q_FUNC_INFO, "KDSelfRestarter instance must be created in main()");
    if (Private::instance)
        Private::instance->d->restartOnQuit = restart;
}

bool KDSelfRestarter::restartOnQuit()
{
    return Private::instance ? Private::instance->d->restartOnQuit : false;
}
