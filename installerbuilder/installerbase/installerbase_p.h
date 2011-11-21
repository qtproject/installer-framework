/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include <QtCore/QThread>
#include <QtGui/QApplication>

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
    InstallerBase(QObject *parent = 0);
    ~InstallerBase();

    int replaceMaintenanceToolBinary(QStringList arguments);
    static void showVersion(int &argc, char **argv, const QString &version);

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

class MyCoreApplication : public QCoreApplication
{
public:
    MyCoreApplication(int &argc, char **argv);
    virtual bool notify(QObject *receiver, QEvent *event);
};

class MyApplication : public QApplication
{
public:
    MyApplication(int &argc, char **argv);
    virtual bool notify(QObject *receiver, QEvent *event);
};
