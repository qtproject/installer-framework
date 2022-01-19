/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
**************************************************************************/

#ifndef DOWNLOADFILETASK_H
#define DOWNLOADFILETASK_H

#include "abstractfiletask.h"
#include "filedownloaderfactory.h"

#include <QAuthenticator>
Q_DECLARE_METATYPE(QAuthenticator)

namespace QInstaller {

namespace TaskRole {
enum
{
    Authenticator = TaskRole::TargetFile + 10
};
}

class AuthenticationRequiredException : public TaskException
{
public:
    enum struct Type {
        Proxy,
        Server
    };

    explicit AuthenticationRequiredException(Type type, const QString &message);
    ~AuthenticationRequiredException() throw() {}

    Type type() const { return m_type; }

    QNetworkProxy proxy() const { return m_proxy; }
    void setProxy(const QNetworkProxy &proxy) { m_proxy = proxy; }

    FileTaskItem taskItem() const { return m_fileTaskItem; }
    void setFileTaskItem(const FileTaskItem &item) { m_fileTaskItem = item; }

    void raise() const override { throw *this; }
    AuthenticationRequiredException *clone() const override {
        return new AuthenticationRequiredException(*this); }

private:
    Type m_type;
    QNetworkProxy m_proxy;
    FileTaskItem m_fileTaskItem;
};

class INSTALLER_EXPORT DownloadFileTask : public AbstractFileTask
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadFileTask)

public:
    DownloadFileTask() {}
    explicit DownloadFileTask(const FileTaskItem &item)
        : AbstractFileTask(item) {}
    explicit DownloadFileTask(const QList<FileTaskItem> &items);

    explicit DownloadFileTask(const QString &source)
        : AbstractFileTask(source) {}
    DownloadFileTask(const QString &source, const QString &target)
        : AbstractFileTask(source, target) {}

    void addTaskItem(const FileTaskItem &items);
    void addTaskItems(const QList<FileTaskItem> &items);

    void setTaskItem(const FileTaskItem &items);
    void setTaskItems(const QList<FileTaskItem> &items);

    void setAuthenticator(const QAuthenticator &authenticator);
    void setProxyFactory(KDUpdater::FileDownloaderProxyFactory *factory);

    void doTask(QFutureInterface<FileTaskResult> &fi) override;

private:
    friend class Downloader;
    QAuthenticator m_authenticator;
    QScopedPointer<KDUpdater::FileDownloaderProxyFactory> m_proxyFactory;
};

}   // namespace QInstaller

#endif // DOWNLOADFILETASK_H
