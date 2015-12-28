/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/

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
