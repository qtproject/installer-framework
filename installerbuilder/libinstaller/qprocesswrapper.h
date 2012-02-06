/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef QPROCESSWRAPPER_H
#define QPROCESSWRAPPER_H

#include <installer_global.h>

#include<QtCore/QIODevice>
#include<QtCore/QObject>
#include<QtCore/QProcess>

class INSTALLER_EXPORT QProcessWrapper : public QObject
{
    Q_OBJECT
public:
    enum ProcessState {
        NotRunning,
        Starting,
        Running
    };

    enum ExitStatus {
        NormalExit,
        CrashExit
    };

    enum ProcessChannel {
        StandardOutput = 0,
        StandardError = 1
    };

    enum ProcessChannelMode {
        SeparateChannels = 0,
        MergedChannels = 1,
        ForwardedChannels = 2
    };

    explicit QProcessWrapper(QObject *parent = 0);
    ~QProcessWrapper();

    void closeWriteChannel();
    int exitCode() const;
    ExitStatus exitStatus() const;
    void kill();
    void terminate();
    QByteArray readAll();
    QByteArray readAllStandardOutput();
    void setWorkingDirectory(const QString &dir);

    void start(const QString &program);
    void start(const QString &program, const QStringList &arguments,
        QIODevice::OpenMode mode = QIODevice::ReadWrite);

    static bool startDetached(const QString &program);
    static bool startDetached(const QString &program, const QStringList &arguments);
    static bool startDetached(const QString &program, const QStringList &arguments,
        const QString &workingDirectory, qint64 *pid = 0);

    ProcessState state() const;
    bool waitForStarted(int msecs = 30000);
    bool waitForFinished(int msecs = 30000);
    void setEnvironment(const QStringList &environment);
    QString workingDirectory() const;
    qint64 write(const QByteArray &byteArray);
    QProcessWrapper::ProcessChannel readChannel() const;
    void setReadChannel(QProcessWrapper::ProcessChannel channel);
    QProcessWrapper::ProcessChannelMode processChannelMode() const;
    void setProcessChannelMode(QProcessWrapper::ProcessChannelMode channel);
#ifdef Q_OS_WIN
    void setNativeArguments(const QString &arguments);
#endif

Q_SIGNALS:
    void bytesWritten(qint64);
    void aboutToClose();
    void readChannelFinished();
    void error(QProcess::ProcessError);
    void readyReadStandardOutput();
    void readyReadStandardError();
    void finished(int exitCode);
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void readyRead();
    void started();
    void stateChanged(QProcess::ProcessState newState);

public Q_SLOTS:
    void cancel();

protected:
    void timerEvent(QTimerEvent *event);

private:
    class Private;
    Private *d;
};

#endif  // QPROCESSWRAPPER_H
