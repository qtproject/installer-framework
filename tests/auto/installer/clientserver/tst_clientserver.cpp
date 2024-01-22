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

#include "../shared/verifyinstaller.h"
#include "../shared/packagemanager.h"

#include <protocol.h>
#include <qprocesswrapper.h>
#include <qsettingswrapper.h>
#include <remoteclient.h>
#include <remotefileengine.h>
#include <remoteserver.h>
#include <fileutils.h>

#ifdef IFW_LIBARCHIVE
#include <libarchivewrapper_p.h>
#endif

#include <QBuffer>
#include <QSettings>
#include <QLocalSocket>
#include <QTest>
#include <QSignalSpy>
#include <QTemporaryFile>
#include <QUuid>
#include <QLocalServer>

using namespace QInstaller;

class MyRemoteObject : public RemoteObject
{
public:
    MyRemoteObject()
        : RemoteObject(QLatin1String("MyRemoteObject")) {};

    ~MyRemoteObject() = default;

    bool connectToServer() { return RemoteObject::connectToServer(); }
};

class tst_ClientServer : public QObject
{
    Q_OBJECT

private:
    template<typename T>
    void sendCommand(QIODevice *device, const QByteArray &cmd, T t)
    {
        QByteArray data;
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << t;
        sendPacket(device, cmd, data);
    }

    template<typename T>
    void receiveCommand(QIODevice *device, QByteArray *cmd, T *t)
    {
        QByteArray data;
        while (!receivePacket(device, cmd, &data))
            device->waitForReadyRead(-1);
        QDataStream stream(&data, QIODevice::ReadOnly);
        stream >> *t;
        QCOMPARE(stream.status(), QDataStream::Ok);
        QVERIFY(stream.atEnd());
    }

private slots:
    void init()
    {
        RemoteClient::instance().setActive(true);
    }

    void sendReceivePacket()
    {
        QByteArray validPackage;
        typedef qint32 PackageSize;

        // first try sendPacket ...
        {
            QBuffer device(&validPackage);
            device.open(QBuffer::WriteOnly);

            const QByteArray cmd = "say";
            const QByteArray data = "hello" ;
            QInstaller::sendPacket(&device, cmd, data);

            // 1 is delimiter (\0)
            QCOMPARE(device.buffer().size(), (int)sizeof(PackageSize) + cmd.size() + 1 + data.size());
            QCOMPARE(device.buffer().right(data.size()), data);
            QCOMPARE(device.buffer().mid(sizeof(PackageSize), cmd.size()), cmd);
        }

        // now try successful receivePacket ...
        {
            QBuffer device(&validPackage);
            device.open(QBuffer::ReadOnly);

            QByteArray cmd;
            QByteArray data;
            QCOMPARE(QInstaller::receivePacket(&device, &cmd, &data), true);

            QCOMPARE(device.pos(), device.size());
            QCOMPARE(cmd, QByteArray("say"));
            QCOMPARE(data, QByteArray("hello"));
        }


        // now try read of incomplete packet ...
        {
            QByteArray incompletePackage = validPackage;
            char toStrip = validPackage.at(validPackage.size() - 1);
            incompletePackage.resize(incompletePackage.size() - 1);
            QBuffer device(&incompletePackage);
            device.open(QBuffer::ReadOnly);

            QByteArray cmd;
            QByteArray data;
            QCOMPARE(QInstaller::receivePacket(&device, &cmd, &data), false);

            QCOMPARE(device.pos(), 0);
            QCOMPARE(cmd, QByteArray());
            QCOMPARE(data, QByteArray());

            // make packet complete again, retry
            device.buffer().append(toStrip);
            QCOMPARE(device.buffer(), validPackage);

            QCOMPARE(QInstaller::receivePacket(&device, &cmd, &data), true);

            QCOMPARE(device.pos(), device.size());
            QCOMPARE(cmd, QByteArray("say"));
            QCOMPARE(data, QByteArray("hello"));
        }
    }

    void localSocket()
    {
        //
        // test roundtrip of a (big) packet via QLocalSocket
        //
        const QString socketName(__FUNCTION__);
        QLocalServer::removeServer(socketName);

        QEventLoop loop;

        const QByteArray command = "HELLO";
        const QByteArray message(10905, '0');

        QLocalServer server;
        // server
        QLocalSocket *rcv = 0;
        auto srvDataArrived = [&]() {
            QByteArray command, message;
            if (!receivePacket(rcv, &command, &message))
                return;
            sendPacket(rcv, command, message);
        };

        connect(&server, &QLocalServer::newConnection, [&,srvDataArrived]() {
            rcv = server.nextPendingConnection();
            connect(rcv, &QLocalSocket::readyRead, srvDataArrived);
        });

        server.listen(socketName);


        QLocalSocket snd;
        // client
        auto clientDataArrived = [&]() {
            QByteArray cmd, msg;
            if (!receivePacket(&snd, &cmd, &msg))
                return;
            QCOMPARE(cmd, command);
            QCOMPARE(msg, message);
            loop.exit();
        };

        connect(&snd, &QLocalSocket::readyRead, clientDataArrived);

        QTimer::singleShot(0, [&]() {
            snd.connect(&snd, &QLocalSocket::connected, [&](){
                sendPacket(&snd,  command, message);
            });
            snd.connectToServer(socketName);
        });

        loop.exec();
    }


    void testServerConnectDebug()
    {
        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();

        server.init(socketName, QString(Protocol::DefaultAuthorizationKey),
            Protocol::Mode::Debug);
        server.start();

        QLocalSocket socket;
        socket.connectToServer(socketName);
        QVERIFY2(socket.waitForConnected(), "Cannot connect to server.");
        QCOMPARE(socket.state() == QLocalSocket::ConnectedState, true);

        sendCommand(&socket, Protocol::Authorize, QString(Protocol::DefaultAuthorizationKey));

        {
            QByteArray command;
            bool authorized;
            receiveCommand(&socket, &command, &authorized);
            QCOMPARE(command, QByteArray(Protocol::Reply));
            QCOMPARE(authorized, true);
        }

        sendCommand(&socket, Protocol::Authorize, QString::fromLatin1("Some Key"));

        {
            QByteArray command;
            bool authorized;
            receiveCommand(&socket, &command, &authorized);
            QCOMPARE(command, QByteArray(Protocol::Reply));
            QCOMPARE(authorized, false);
        }
    }

    void testServerConnectRelease()
    {
        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QString("SomeKey"), Protocol::Mode::Production);
        server.start();

        QLocalSocket socket;
        socket.connectToServer(socketName);
        QVERIFY2(socket.waitForConnected(), "Cannot connect to server.");
        QCOMPARE(socket.state() == QLocalSocket::ConnectedState, true);

        sendCommand(&socket, Protocol::Authorize, QString::fromLatin1("SomeKey"));

        {
            QByteArray command;
            bool authorized;
            receiveCommand(&socket, &command, &authorized);
            QCOMPARE(command, QByteArray(Protocol::Reply));
            QCOMPARE(authorized, true);
        }

        sendCommand(&socket, Protocol::Authorize, QString::fromLatin1(Protocol::DefaultAuthorizationKey));

        {
            QByteArray command;
            bool authorized;
            receiveCommand(&socket, &command, &authorized);
            QCOMPARE(command, QByteArray(Protocol::Reply));
            QCOMPARE(authorized, false);
        }
    }

    void testCreateDestroyRemoteObject()
    {
        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Production);
        server.start();

        RemoteClient::instance().init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Debug,
                                      Protocol::StartAs::User);

        // Look for warnings on connection and disconnection..
        qInstallMessageHandler(exitOnWarningMessageHandler);

        MyRemoteObject *object = new MyRemoteObject;
        QVERIFY(!object->isConnectedToServer());
        delete object;

        object = new MyRemoteObject;
        QVERIFY(object->connectToServer());
        QVERIFY(object->isConnectedToServer());
        delete object;

        object = new MyRemoteObject;
        QVERIFY(object->connectToServer());
        QVERIFY(object->isConnectedToServer());

        RemoteClient::instance().setActive(false);
        QVERIFY(!object->isConnectedToServer());
        delete object;
    }

    void testQSettingsWrapper_data()
    {
        QTest::addColumn<QSettings::Format>("format");
        QTest::newRow("Native format") << QSettings::NativeFormat;
        QTest::newRow("INI format") << QSettings::IniFormat;
    }

    void testQSettingsWrapper()
    {
        QFETCH(QSettings::Format, format);

        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Production);
        server.start();

        RemoteClient::instance().init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Debug,
                                      Protocol::StartAs::User);

        QSettingsWrapper wrapper(format, QSettingsWrapper::UserScope, "digia", "clientserver");

        QCOMPARE(wrapper.isConnectedToServer(), false);
        wrapper.clear();
        QCOMPARE(wrapper.isConnectedToServer(), true);
        wrapper.sync();
        wrapper.setFallbacksEnabled(false);

        QSettings settings(format, QSettings::UserScope, "digia", "clientserver");
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

        const QDateTime dateTime = QDateTime::currentDateTimeUtc();
        // The wrapper does not support this method, but assume:
        // QCOMPARE(wrapper.iniCodec(), QTextCodec::codecForName("UTF-8"));
        wrapper.setValue("dateTime", dateTime);
        wrapper.sync();
        QCOMPARE(wrapper.value("dateTime").toDateTime(), dateTime);
        QCOMPARE(settings.value("dateTime").toDateTime(), dateTime);

        wrapper.remove("dateTime");
        wrapper.sync();
        QCOMPARE(wrapper.contains("dateTime"), false);
        QCOMPARE(settings.contains("dateTime"), false);

        settings.setValue("dateTime", dateTime);
        settings.sync();
        QCOMPARE(wrapper.value("dateTime").toDateTime(), dateTime);
        QCOMPARE(settings.value("dateTime").toDateTime(), dateTime);

        settings.remove("dateTime");
        settings.sync();
        QCOMPARE(wrapper.contains("dateTime"), false);
        QCOMPARE(settings.contains("dateTime"), false);

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
        #ifdef Q_OS_LINUX
            QSKIP("This test failes in CI redhat");
        #endif
        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Production);
        server.start();

        RemoteClient::instance().init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Debug,
                                      Protocol::StartAs::User);
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
            QSignalSpy spy2(&wrapper, SIGNAL(finished(int, QProcess::ExitStatus)));

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
            QCOMPARE(arguments.last().toInt(), int(QProcessWrapper::NormalExit));

            QFile::remove(fileName);
        }
    }

    void testRemoteFileEngine()
    {
        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Production);
        server.start();

        RemoteClient::instance().init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Debug,
                                      Protocol::StartAs::User);

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
        file.readLine();
        file.seek(0);
        QCOMPARE(file.atEnd(), false);

        QByteArray ba2(32 * 1024 * 1024, '\0');
        file.readLine(ba2.data(), ba2.size());

        file.resize(0);
        file.write(QProcess::systemEnvironment().join(QLatin1String("\n")).toLocal8Bit());
        QCOMPARE(file.atEnd(), true);
    }

    void testArchiveWrapper_data()
    {
        QTest::addColumn<QString>("suffix");
        QTest::newRow("ZIP archive") << ".zip";
        QTest::newRow("uncompressed tar archive") << ".tar";
        QTest::newRow("gzip compressed tar archive") << ".tar.gz";
        QTest::newRow("bzip2 compressed tar archive") << ".tar.bz2";
        QTest::newRow("xz compressed tar archive") << ".tar.xz";
        QTest::newRow("7zip archive") << ".7z";
    }

    void testArchiveWrapper()
    {
#ifndef IFW_LIBARCHIVE
        QSKIP("Installer Framework built without libarchive support");
#else
        QFETCH(QString, suffix);

        const QString archiveName = generateTemporaryFileName() + suffix;
        const QString targetName = QDir::tempPath() + "/tst_archivewrapper/";

        QTemporaryFile source;
        source.open();
        source.write("Source File");
        source.setAutoRemove(false);

        RemoteServer server;
        QString socketName = QUuid::createUuid().toString();
        server.init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Production);
        server.start();

        RemoteClient::instance().init(socketName, QLatin1String("SomeKey"), Protocol::Mode::Debug,
                                      Protocol::StartAs::User);

        LibArchiveWrapperPrivate archive;
        QCOMPARE(archive.isConnectedToServer(), false);
        archive.setFilename(archiveName);
        QCOMPARE(archive.isConnectedToServer(), true);

        QVERIFY(archive.open(QIODevice::ReadWrite));
        QVERIFY(archive.create(QStringList() << source.fileName()));
        QVERIFY(QFileInfo::exists(archiveName));

        QVERIFY(archive.extract(targetName, 1));
        const QString sourceFilename = QFileInfo(source.fileName()).fileName();
        QVERIFY(QFileInfo::exists(targetName + sourceFilename));
        VerifyInstaller::verifyFileContent(targetName + sourceFilename, source.readAll());
        archive.close();

        QVERIFY(source.remove());
        QVERIFY(QFile::remove(archiveName));
        removeDirectory(targetName);
#endif
    }

    void cleanupTestCase()
    {
        RemoteClient::instance().setActive(false);
        RemoteClient::instance().shutdown();
    }
};

QTEST_MAIN(tst_ClientServer)

#include "tst_clientserver.moc"
