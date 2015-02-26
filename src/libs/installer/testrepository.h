/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
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
**
** $QT_END_LICENSE$
**
**************************************************************************/
#ifndef TESTREPOSITORY_H
#define TESTREPOSITORY_H

#include "qinstallerglobal.h"

#include <repository.h>
#include <settings.h>

#include <kdjob.h>

QT_BEGIN_NAMESPACE
class QAuthenticator;
class QLocale;
class QVariant;
QT_END_NAMESPACE

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {
    class PackageManagerCore;
}

namespace QInstaller {

class INSTALLER_EXPORT TestRepository : public KDJob
{
    Q_OBJECT

public:

    explicit TestRepository(QObject *parent = 0);
    ~TestRepository();

    QInstaller::Repository repository() const;
    void setRepository(const QInstaller::Repository &repository);

private:
    void doStart();
    void doCancel();

private Q_SLOTS:
    void downloadCompleted();
    void downloadAborted(const QString &reason);
    void onAuthenticatorChanged(const QAuthenticator &authenticator);

private:
    QInstaller::Repository m_repository;
    KDUpdater::FileDownloader *m_downloader;
};

} //namespace QInstaller

#endif  // TESTREPOSITORY_H
