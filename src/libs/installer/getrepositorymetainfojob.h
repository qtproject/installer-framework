/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#ifndef GETREPOSITORYMETAINFOJOB_H
#define GETREPOSITORYMETAINFOJOB_H

#include "fileutils.h"
#include "installer_global.h"
#include "repository.h"

#include "kdjob.h"

#include <QtCore/QPointer>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QThreadPool>

#include <QAuthenticator>

namespace KDUpdater {
    class FileDownloader;
}

namespace QInstaller {

class GetRepositoriesMetaInfoJob;
class PackageManagerCore;

class INSTALLER_EXPORT GetRepositoryMetaInfoJob : public KDJob
{
    Q_OBJECT
    class ZipRunnable;
    friend class QInstaller::GetRepositoriesMetaInfoJob;

public:
    explicit GetRepositoryMetaInfoJob(PackageManagerCore *core, QObject *parent = 0);
    ~GetRepositoryMetaInfoJob();

    Repository repository() const;
    void setRepository(const Repository &r);

    int silentRetries() const;
    void setSilentRetries(int retries);

    QString temporaryDirectory() const;
    QString releaseTemporaryDirectory() const;

private:
    /* reimp */ void doStart();
    /* reimp */ void doCancel();
    void finished(int error, const QString &errorString = QString());
    bool updateRepositories(QSet<Repository> *repositories, const QString &username,
                            const QString &password, const QString &displayname = QString());

private Q_SLOTS:
    void startUpdatesXmlDownload();
    void updatesXmlDownloadCanceled();
    void updatesXmlDownloadFinished();
    void updatesXmlDownloadError(const QString &error);

    void fetchNextMetaInfo();
    void metaDownloadCanceled();
    void metaDownloadFinished();
    void metaDownloadError(const QString &error);

    void unzipFinished(bool status, const QString &error);
    void onAuthenticatorChanged(const QAuthenticator &authenticator);

private:
    bool m_canceled;
    int m_silentRetries;
    int m_retriesLeft;
    Repository m_repository;
    QStringList m_packageNames;
    QStringList m_packageVersions;
    QStringList m_packageHash;
    QPointer<KDUpdater::FileDownloader> m_downloader;
    QString m_currentPackageName;
    QString m_currentPackageVersion;
    QString m_temporaryDirectory;
    mutable TempDirDeleter m_tempDirDeleter;

    bool m_waitForDone;
    QThreadPool m_threadPool;
    PackageManagerCore *m_core;
};

}   // namespace QInstaller

#endif // GETREPOSITORYMETAINFOJOB_H
