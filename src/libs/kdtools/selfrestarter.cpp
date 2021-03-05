/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "selfrestarter.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QProcess>

class SelfRestarter::Private
{
public:
    Private(int argc, char *argv[])
        : executable(QString::fromLocal8Bit(argv[0]))
        , restartOnQuit(false)
        , workingPath(QDir::currentPath())
    {

        for (int i = 1; i < argc; ++i)
            args << QString::fromLocal8Bit(argv[i]);
    }

    Private()
        : executable(qApp->applicationFilePath())
        , args(qApp->arguments().mid(1))
        , restartOnQuit(false)
        , workingPath(QDir::currentPath())
    {
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
    static SelfRestarter *instance;
};

SelfRestarter *SelfRestarter::Private::instance = 0;

SelfRestarter::SelfRestarter(int argc, char *argv[])
    : d(new Private(argc, argv))
{
    Q_ASSERT_X(!Private::instance, Q_FUNC_INFO, "Cannot create more than one SelfRestarter instance");
    Private::instance = this;
}

SelfRestarter::~SelfRestarter()
{
    Q_ASSERT_X(Private::instance == this, Q_FUNC_INFO, "Cannot create more than one SelfRestarter instance");
    delete d;
    Private::instance = 0;
}

void SelfRestarter::setRestartOnQuit(bool restart)
{
    Q_ASSERT_X(Private::instance, Q_FUNC_INFO, "SelfRestarter instance must be created in main()");
    if (Private::instance)
        Private::instance->d->restartOnQuit = restart;
}

bool SelfRestarter::restartOnQuit()
{
    return Private::instance ? Private::instance->d->restartOnQuit : false;
}
