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

#ifndef INSTALLERBASE_P_H
#define INSTALLERBASE_P_H

#include <QThread>

namespace KDUpdater {
    class FileDownloader;
}

QT_BEGIN_NAMESPACE
class QFile;
QT_END_NAMESPACE

class Sleep : public QThread
{
public:
    static void sleep(unsigned long ms)
    {
        QThread::usleep(ms);
    }
};

class InstallerBase : public QObject
{
    Q_OBJECT

public:
    explicit InstallerBase(QObject *parent = 0);
    ~InstallerBase();

    int replaceMaintenanceToolBinary(QStringList arguments);

    static void showUsage();
    static void showVersion(const QString &version);

private slots:
    void downloadStarted();
    void downloadFinished();
    void downloadProgress(double progress);
    void downloadAborted(const QString &error);

private:
    void deferredRename(const QString &source, const QString &target);
    void writeMaintenanceBinary(const QString &target, QFile *source, qint64 size);

private:
    volatile bool m_downloadFinished;
    QScopedPointer<KDUpdater::FileDownloader> m_downloader;
};

#endif  // INSTALLERBASE_P_H
