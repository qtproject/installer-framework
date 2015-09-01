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
#include "testrepository.h"

#include "packagemanagercore.h"
#include "packagemanagerproxyfactory.h"
#include "proxycredentialsdialog.h"
#include "serverauthenticationdialog.h"

#include <QFile>

namespace QInstaller {

TestRepository::TestRepository(PackageManagerCore *parent)
    : Job(parent)
    , m_core(parent)
{
    setAutoDelete(false);
    setCapabilities(Cancelable);

    connect(&m_timer, &QTimer::timeout, this, &TestRepository::onTimeout);
    connect(&m_xmlTask, &QFutureWatcherBase::finished, this, &TestRepository::downloadCompleted);
}

TestRepository::~TestRepository()
{
    reset();
}

Repository TestRepository::repository() const
{
    return m_repository;
}

void TestRepository::setRepository(const Repository &repository)
{
    reset();
    m_repository = repository;
}

void TestRepository::doStart()
{
    reset();
    if (!m_core) {
        emitFinishedWithError(Job::Canceled, tr("Missing package manager core engine."));
        return; // We can't do anything here without core, so avoid tons of !m_core checks.
    }

    const QUrl url = m_repository.url();
    if (url.isEmpty()) {
        emitFinishedWithError(QInstaller::InvalidUrl, tr("Empty repository URL."));
        return;
    }

    QAuthenticator auth;
    auth.setUser(m_repository.username());
    auth.setPassword(m_repository.password());

    FileTaskItem item(m_repository.url().toString() + QLatin1String("/Updates.xml?") +
        QString::number(qrand() * qrand()));
    item.insert(TaskRole::Authenticator, QVariant::fromValue(auth));

    m_timer.start(10000);
    DownloadFileTask *const xmlTask = new DownloadFileTask(item);
    if (m_core)
        xmlTask->setProxyFactory(m_core->proxyFactory());
    m_xmlTask.setFuture(QtConcurrent::run(&DownloadFileTask::doTask, xmlTask));
}

void TestRepository::doCancel()
{
    reset();
    emitFinishedWithError(Job::Canceled, tr("Download canceled."));
}

void TestRepository::onTimeout()
{
    reset();
    emitFinishedWithError(Job::Canceled, tr("Timeout while testing repository \"%1\".")
        .arg(m_repository.displayname()));
}

void TestRepository::downloadCompleted()
{
    if (error() != Job::NoError)
        return;

    try {
        m_xmlTask.waitForFinished();

        m_timer.stop();
        QFile file(m_xmlTask.future().results().value(0).target());
        if (file.open(QIODevice::ReadOnly)) {
            QDomDocument doc;
            QString errorMsg;
            if (!doc.setContent(&file, &errorMsg)) {
                emitFinishedWithError(QInstaller::InvalidUpdatesXml,
                    tr("Cannot parse Updates.xml: %1").arg(errorMsg));
            } else {
                emitFinishedWithError(Job::NoError, QString(/*Success*/)); // OPK
            }
        } else {
            emitFinishedWithError(QInstaller::DownloadError,
                tr("Cannot open Updates.xml for reading: %1").arg(file.errorString()));
        }
    } catch (const AuthenticationRequiredException &e) {
        m_timer.stop();
        if (e.type() == AuthenticationRequiredException::Type::Server) {
            ServerAuthenticationDialog dlg(e.message(), e.taskItem());
            if (dlg.exec() == QDialog::Accepted) {
                m_repository.setUsername(dlg.user());
                m_repository.setPassword(dlg.password());
            }
            QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
            return;
        } else if (e.type() == AuthenticationRequiredException::Type::Proxy) {
            const QNetworkProxy proxy = e.proxy();
            ProxyCredentialsDialog proxyCredentials(proxy);
            if (proxyCredentials.exec() == QDialog::Accepted) {
                PackageManagerProxyFactory *factory = m_core->proxyFactory();
                factory->setProxyCredentials(proxy, proxyCredentials.userName(),
                    proxyCredentials.password());
                m_core->setProxyFactory(factory);
            }
            QMetaObject::invokeMethod(this, "doStart", Qt::QueuedConnection);
            return;
        } else {
            emitFinishedWithError(QInstaller::DownloadError, tr("Authentication failed."));
        }
    } catch (const TaskException &e) {
        m_timer.stop();
        emitFinishedWithError(QInstaller::DownloadError, e.message());
    } catch (const QUnhandledException &e) {
        m_timer.stop();
        emitFinishedWithError(QInstaller::DownloadError, QLatin1String(e.what()));
    } catch (...) {
        m_timer.stop();
        emitFinishedWithError(QInstaller::DownloadError,
            tr("Unknown error while testing repository \"%1\".").arg(m_repository.displayname()));
    }
}


// -- private

void TestRepository::reset()
{
    m_timer.stop();
    setError(NoError);
    setErrorString(QString());

    try {
        if (m_xmlTask.isRunning())
            m_xmlTask.cancel();
    } catch (...) {}
}

} // namespace QInstaller
