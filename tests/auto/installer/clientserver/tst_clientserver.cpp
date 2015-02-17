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

#include <protocol.h>
#include <qprocesswrapper.h>
#include <qsettingswrapper.h>
#include <remoteclient.h>
#include <remotefileengine.h>
#include <remoteserver.h>

#include <QSettings>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QTest>
#include <QSignalSpy>

using namespace QInstaller;

class tst_ClientServer : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase()
    {
        RemoteClient::instance().setActive(true);
    }

    void testServerConnectDebug()
    {
        RemoteServer server;
        server.init(Protocol::DefaultPort, QString(Protocol::DefaultAuthorizationKey),
            Protocol::Mode::Debug);
        server.start();

        QTcpSocket socket;
        socket.connectToHost(QLatin1String(Protocol::DefaultHostAddress), Protocol::DefaultPort);
        QVERIFY2(socket.waitForConnected(), "Could not connect to server.");
        QCOMPARE(socket.state() == QAbstractSocket::ConnectedState, true);

        QDataStream stream;
        stream.setDevice(&socket);
        stream << QString::fromLatin1(Protocol::Authorize) << QString(Protocol::DefaultAuthorizationKey);

        socket.waitForBytesWritten(-1);
        if (!socket.bytesAvailable())
            socket.waitForReadyRead(-1);

        quint32 size; stream >> size;
        bool authorized;
        stream >> authorized;
        QCOMPARE(authorized, true);

        socket.flush();
        stream << QString::fromLatin1(Protocol::Authorize) << QString("SomeKey");
        socket.waitForBytesWritten(-1);
        if (!socket.bytesAvailable())
            socket.waitForReadyRead(-1);

        stream >> size;
        stream >> authorized;
        QCOMPARE(authorized, false);
    }

    void testServerConnectRelease()
    {
        RemoteServer server;
        quint16 port = (30000 + qrand() % 100);
        server.init(port, QString("SomeKey"), Protocol::Mode::Production);
        server.start();

        QTcpSocket socket;
        socket.connectToHost(QLatin1String(Protocol::DefaultHostAddress), port);
        QVERIFY2(socket.waitForConnected(), "Could not connect to server.");
        QCOMPARE(socket.state() == QAbstractSocket::ConnectedState, true);

        QDataStream stream;
        stream.setDevice(&socket);
        stream << QString::fromLatin1(Protocol::Authorize) << QString("SomeKey");

        socket.waitForBytesWritten(-1);
        if (!socket.bytesAvailable())
            socket.waitForReadyRead(-1);

        quint32 size; stream >> size;
        bool authorized;
        stream >> authorized;
        QCOMPARE(authorized, true);

        socket.flush();
        stream << QString::fromLatin1(Protocol::Authorize) << QString(Protocol::DefaultAuthorizationKey);
        socket.waitForBytesWritten(-1);
        if (!socket.bytesAvailable())
            socket.waitForReadyRead(-1);

        stream >> size;
        stream >> authorized;
        QCOMPARE(authorized, false);
    }

    void testQSettingsWrapper()
    {
        RemoteServer server;
        server.start();

        QSettingsWrapper wrapper("digia", "clientserver");
        QCOMPARE(wrapper.isConnectedToServer(), false);
        wrapper.clear();
        QCOMPARE(wrapper.isConnectedToServer(), true);
        wrapper.sync();
        wrapper.setFallbacksEnabled(false);

        QSettings settings("digia", "clientserver");
        settings.setFallbacksEnabled(false);

        QCOMPARE(settings.fileName(), wrapper.fileName());
        QCOMPARE(int(settings.format()), int(wrapper.format()));
        QCOMPARE(int(settings.scope()), int(wrapper.scope()));
        QCOMPARE(settings.organizationName(), wrapper.organizationName());
        QCOMPARE(settings.applicationName(), wrapper.applicationName());
        QCOMPARE(settings.fallbacksEnabled(), wrapper.fallbacksEnabled());

        wrapper.setValue("key", "value");
        wrapper.setValue("contains", "value");
        wrapper.sync();

        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));

        QCOMPARE(wrapper.contains("contains"), true);
        QCOMPARE(settings.contains("contains"), true);
        wrapper.remove("contains");
        wrapper.sync();
        QCOMPARE(wrapper.contains("contains"), false);
        QCOMPARE(settings.contains("contains"), false);

        wrapper.clear();
        wrapper.sync();
        QCOMPARE(wrapper.contains("key"), false);
        QCOMPARE(settings.contains("key"), false);

        wrapper.beginGroup("group");
        wrapper.setValue("key", "value");
        wrapper.endGroup();
        wrapper.sync();

        wrapper.beginGroup("group");
        settings.beginGroup("group");
        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));
        QCOMPARE(wrapper.group(), QLatin1String("group"));
        QCOMPARE(settings.group(), QLatin1String("group"));
        settings.endGroup();
        wrapper.endGroup();

        wrapper.beginWriteArray("array");
        wrapper.setArrayIndex(0);
        wrapper.setValue("key", "value");
        wrapper.endArray();
        wrapper.sync();

        wrapper.beginReadArray("array");
        settings.beginReadArray("array");
        wrapper.setArrayIndex(0);
        settings.setArrayIndex(0);
        QCOMPARE(wrapper.value("key").toString(), QLatin1String("value"));
        QCOMPARE(settings.value("key").toString(), QLatin1String("value"));
        settings.endArray();
        wrapper.endArray();

        wrapper.setValue("fridge/color", 3);
        wrapper.setValue("fridge/size", QSize(32, 96));
        wrapper.setValue("sofa", true);
        wrapper.setValue("tv", false);

        wrapper.remove("group");
        wrapper.remove("array");
        wrapper.sync();

        QStringList keys = wrapper.allKeys();
        QCOMPARE(keys.count(), 4);
        QCOMPARE(keys.contains("fridge/color"), true);
        QCOMPARE(keys.contains("fridge/size"), true);
        QCOMPARE(keys.contains("sofa"), true);
        QCOMPARE(keys.contains("tv"), true);

        wrapper.beginGroup("fridge");
        keys = wrapper.allKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("color"), true);
        QCOMPARE(keys.contains("size"), true);
        wrapper.endGroup();

        keys = wrapper.childKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("sofa"), true);
        QCOMPARE(keys.contains("tv"), true);

        wrapper.beginGroup("fridge");
        keys = wrapper.childKeys();
        QCOMPARE(keys.count(), 2);
        QCOMPARE(keys.contains("color"), true);
        QCOMPARE(keys.contains("size"), true);
        wrapper.endGroup();

        QStringList groups = wrapper.childGroups();
        QCOMPARE(groups.count(), 1);
        QCOMPARE(groups.contains("fridge"), true);

        wrapper.beginGroup("fridge");
        groups = wrapper.childGroups();
        QCOMPARE(groups.count(), 0);
        wrapper.endGroup();
    }

    void testQProcessWrapper()
    {
        RemoteServer server;
        server.start();

        {
            QProcess process;
            QProcessWrapper wrapper;

            QCOMPARE(wrapper.isConnectedToServer(), false);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(wrapper.isConnectedToServer(), true);

            QCOMPARE(process.workingDirectory(), wrapper.workingDirectory());
            process.setWorkingDirectory(QDir::tempPath());
            wrapper.setWorkingDirectory(QDir::tempPath());
            QCOMPARE(process.workingDirectory(), wrapper.workingDirectory());

            QCOMPARE(process.environment(), wrapper.environment());
            process.setEnvironment(QProcess::systemEnvironment());
            wrapper.setEnvironment(QProcess::systemEnvironment());
            QCOMPARE(process.environment(), wrapper.environment());

            QCOMPARE(int(process.readChannel()), int(wrapper.readChannel()));
            process.setReadChannel(QProcess::StandardError);
            wrapper.setReadChannel(QProcessWrapper::StandardError);
            QCOMPARE(int(process.readChannel()), int(wrapper.readChannel()));

            QCOMPARE(int(process.processChannelMode()), int(wrapper.processChannelMode()));
            process.setProcessChannelMode(QProcess::ForwardedChannels);
            wrapper.setProcessChannelMode(QProcessWrapper::ForwardedChannels);
            QCOMPARE(int(process.processChannelMode()), int(wrapper.processChannelMode()));
        }

        {
            QProcessWrapper wrapper;

            QCOMPARE(wrapper.isConnectedToServer(), false);
            QCOMPARE(int(wrapper.exitCode()), 0);
            QCOMPARE(wrapper.isConnectedToServer(), true);

            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(int(wrapper.exitStatus()), int(QProcessWrapper::NormalExit));

            QString fileName;
            {
                QTemporaryFile file(QDir::tempPath() +
#ifdef Q_OS_WIN
                    QLatin1String("/XXXXXX.bat")
#else
                    QLatin1String("/XXXXXX.sh")
#endif
                );
                file.setAutoRemove(false);
                QCOMPARE(file.open(), true);
#ifdef Q_OS_WIN
                file.write("@echo off\necho Mega test output!");
#else
                file.write("#!/bin/bash\necho Mega test output!");
#endif
                file.setPermissions(file.permissions() | QFile::ExeOther | QFile::ExeGroup
                    | QFile::ExeUser);
                fileName = file.fileName();
            }

            QSignalSpy spy(&wrapper, SIGNAL(started()));
            QSignalSpy spy2(&wrapper, SIGNAL(finished(int)));
            QSignalSpy spy3(&wrapper, SIGNAL(finished(int, QProcess::ExitStatus)));

#ifdef Q_OS_WIN
            wrapper.start(fileName);
#else
            wrapper.start("sh", QStringList() << fileName);
#endif
            QCOMPARE(wrapper.waitForStarted(), true);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::Running));
            QCOMPARE(wrapper.waitForFinished(), true);
            QCOMPARE(int(wrapper.state()), int(QProcessWrapper::NotRunning));
            QCOMPARE(wrapper.readAll().trimmed(), QByteArray("Mega test output!"));

            QTest::qWait(500);

            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy2.count(), 1);
            QList<QVariant> arguments = spy2.takeFirst();
            QCOMPARE(arguments.first().toInt(), 0);

            QCOMPARE(spy3.count(), 1);
            arguments = spy3.takeFirst();
            QCOMPARE(arguments.first().toInt(), 0);
            QCOMPARE(arguments.last().toInt(), int(QProcessWrapper::NormalExit));

            QFile::remove(fileName);
        }
    }

    void testRemoteFileEngine()
    {
        RemoteServer server;
        server.start();

        QString filename;
        {
            QTemporaryFile file;
            file.setAutoRemove(false);
            QCOMPARE(file.open(), true);
            file.write(QProcess::systemEnvironment().join(QLatin1String("\n")).toLocal8Bit());
            filename = file.fileName();
        }

        RemoteFileEngineHandler handler;

        QFile file;
        file.setFileName(filename);
        file.open(QIODevice::ReadWrite);
        const QByteArray ba = file.readLine();
        file.seek(0);
        QCOMPARE(file.atEnd(), false);

        QByteArray ba2(32 * 1024 * 1024, '\0');
        file.readLine(ba2.data(), ba2.size());

        file.resize(0);
        file.write(QProcess::systemEnvironment().join(QLatin1String("\n")).toLocal8Bit());
        QCOMPARE(file.atEnd(), true);
    }

    void cleanupTestCase()
    {
        RemoteClient::instance().setActive(false);
        RemoteClient::instance().shutdown();
    }
};

QTEST_MAIN(tst_ClientServer)

#include "tst_clientserver.moc"
