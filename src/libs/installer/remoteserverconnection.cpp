/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "remoteserverconnection.h"

#include "errors.h"
#include "protocol.h"
#include "remoteserverconnection_p.h"
#include "utils.h"
#include "permissionsettings.h"

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>

namespace QInstaller {

RemoteServerConnection::RemoteServerConnection(qintptr socketDescriptor, const QString &key,
                                               QObject *parent)
    : QThread(parent)
    , m_socketDescriptor(socketDescriptor)
    , m_process(nullptr)
    , m_engine(nullptr)
    , m_authorizationKey(key)
    , m_signalReceiver(nullptr)
{
    setObjectName(QString::fromLatin1("RemoteServerConnection(%1)").arg(socketDescriptor));
}

// Helper RAII to ensure stream data was correctly (and completely) read
struct StreamChecker {
    StreamChecker(QDataStream *stream) : stream(stream) {}
    ~StreamChecker() {
        Q_ASSERT(stream->status() == QDataStream::Ok);
        Q_ASSERT(stream->atEnd());
    }
private:
    QDataStream *stream;
};

void RemoteServerConnection::run()
{
    QLocalSocket socket;
    socket.setSocketDescriptor(m_socketDescriptor);
    QScopedPointer<PermissionSettings> settings;

    bool authorized = false;
    while (socket.state() == QLocalSocket::ConnectedState) {
        QByteArray cmd;
        QByteArray data;

        if (!receivePacket(&socket, &cmd, &data)) {
            socket.waitForReadyRead(250);
            continue;
        }

        const QString command = QString::fromLatin1(cmd);
        QBuffer buf;
        buf.setBuffer(&data);
        buf.open(QIODevice::ReadOnly);
        QDataStream stream;
        stream.setDevice(&buf);
        StreamChecker streamChecker(&stream);

        if (authorized && command == QLatin1String(Protocol::Shutdown)) {
            authorized = false;
            sendData(&socket, true);
            socket.flush();
            socket.close();
            emit shutdownRequested();
            return;
        } else if (command == QLatin1String(Protocol::Authorize)) {
            QString key;
            stream >> key;
            sendData(&socket, (authorized = (key == m_authorizationKey)));
            socket.flush();
            if (!authorized) {
                socket.close();
                return;
            }
        } else if (authorized) {
            if (command.isEmpty())
                continue;

            if (command == QLatin1String(Protocol::Create)) {
                QString type;
                stream >> type;
                if (type == QLatin1String(Protocol::QSettings)) {
                    QVariant application;
                    QVariant organization;
                    QVariant scope, format;
                    QVariant fileName;
                    stream >> application; stream >> organization; stream >> scope; stream >> format;
                    stream >> fileName;

                    if (fileName.toString().isEmpty()) {
                        settings.reset(new PermissionSettings(QSettings::Format(format.toInt()),
                            QSettings::Scope(scope.toInt()), organization.toString(), application
                            .toString()));
                    } else {
                        settings.reset(new PermissionSettings(fileName.toString(), QSettings::Format(format.toInt())));
                    }
                } else if (type == QLatin1String(Protocol::QProcess)) {
                    if (m_process)
                        m_process->deleteLater();
                    m_process = new QProcess;
                    m_signalReceiver = new QProcessSignalReceiver(m_process);
                } else if (type == QLatin1String(Protocol::QAbstractFileEngine)) {
                    if (m_engine)
                        delete m_engine;
                    m_engine = new QFSFileEngine;
                }
                continue;
            }

            if (command == QLatin1String(Protocol::Destroy)) {
                QString type;
                stream >> type;
                if (type == QLatin1String(Protocol::QSettings)) {
                    settings.reset();
                } else if (type == QLatin1String(Protocol::QProcess)) {
                    m_signalReceiver->m_receivedSignals.clear();
                    m_process->deleteLater();
                    m_process = nullptr;
                } else if (type == QLatin1String(Protocol::QAbstractFileEngine)) {
                    delete m_engine;
                    m_engine = nullptr;
                }
                return;
            }

            if (command == QLatin1String(Protocol::GetQProcessSignals)) {
                if (m_signalReceiver) {
                    QMutexLocker _(&m_signalReceiver->m_lock);
                    sendData(&socket, m_signalReceiver->m_receivedSignals);
                    socket.flush();
                    m_signalReceiver->m_receivedSignals.clear();
                }
                continue;
            }

            if (command.startsWith(QLatin1String(Protocol::QProcess))) {
                handleQProcess(&socket, command, stream);
            } else if (command.startsWith(QLatin1String(Protocol::QSettings))) {
                handleQSettings(&socket, command, stream, settings.data());
            } else if (command.startsWith(QLatin1String(Protocol::QAbstractFileEngine))) {
                handleQFSFileEngine(&socket, command, stream);
            } else {
                qDebug() << "Unknown command:" << command;
            }
            socket.flush();
        } else {
            // authorization failed, connection not wanted
            socket.close();
            qDebug() << "Unknown command:" << command;
            return;
        }
    }
}

template <typename T>
void RemoteServerConnection::sendData(QIODevice *device, const T &data)
{
    QByteArray result;
    QDataStream returnStream(&result, QIODevice::WriteOnly);
    returnStream << data;

    sendPacket(device, Protocol::Reply, result);
}

void RemoteServerConnection::handleQProcess(QIODevice *socket, const QString &command, QDataStream &data)
{
    if (command == QLatin1String(Protocol::QProcessCloseWriteChannel)) {
        m_process->closeWriteChannel();
    } else if (command == QLatin1String(Protocol::QProcessExitCode)) {
        sendData(socket, m_process->exitCode());
    } else if (command == QLatin1String(Protocol::QProcessExitStatus)) {
        sendData(socket, static_cast<qint32> (m_process->exitStatus()));
    } else if (command == QLatin1String(Protocol::QProcessKill)) {
        m_process->kill();
    } else if (command == QLatin1String(Protocol::QProcessReadAll)) {
        sendData(socket, m_process->readAll());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardOutput)) {
        sendData(socket, m_process->readAllStandardOutput());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardError)) {
        sendData(socket, m_process->readAllStandardError());
    } else if (command == QLatin1String(Protocol::QProcessStartDetached)) {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        data >> program;
        data >> arguments;
        data >> workingDirectory;

        qint64 pid = -1;
        bool success = QInstaller::startDetached(program, arguments, workingDirectory, &pid);
        sendData(socket, qMakePair< bool, qint64>(success, pid));
    } else if (command == QLatin1String(Protocol::QProcessSetWorkingDirectory)) {
        QString dir;
        data >> dir;
        m_process->setWorkingDirectory(dir);
    } else if (command == QLatin1String(Protocol::QProcessSetEnvironment)) {
        QStringList env;
        data >> env;
        m_process->setEnvironment(env);
    } else if (command == QLatin1String(Protocol::QProcessEnvironment)) {
        sendData(socket, m_process->environment());
    } else if (command == QLatin1String(Protocol::QProcessStart3Arg)) {
        QString program;
        QStringList arguments;
        qint32 mode;
        data >> program;
        data >> arguments;
        data >> mode;
        m_process->start(program, arguments, static_cast<QIODevice::OpenMode> (mode));
    } else if (command == QLatin1String(Protocol::QProcessStart2Arg)) {
        QString program;
        qint32 mode;
        data >> program;
        data >> mode;
        m_process->start(program, static_cast<QIODevice::OpenMode> (mode));
    } else if (command == QLatin1String(Protocol::QProcessState)) {
        sendData(socket, static_cast<qint32> (m_process->state()));
    } else if (command == QLatin1String(Protocol::QProcessTerminate)) {
        m_process->terminate();
    } else if (command == QLatin1String(Protocol::QProcessWaitForFinished)) {
        qint32 msecs;
        data >> msecs;
        sendData(socket, m_process->waitForFinished(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWaitForStarted)) {
        qint32 msecs;
        data >> msecs;
        sendData(socket, m_process->waitForStarted(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWorkingDirectory)) {
        sendData(socket, m_process->workingDirectory());
    } else if (command == QLatin1String(Protocol::QProcessErrorString)) {
        sendData(socket, m_process->errorString());
    } else if (command == QLatin1String(Protocol::QProcessReadChannel)) {
        sendData(socket, static_cast<qint32> (m_process->readChannel()));
    } else if (command == QLatin1String(Protocol::QProcessSetReadChannel)) {
        qint32 processChannel;
        data >> processChannel;
        m_process->setReadChannel(static_cast<QProcess::ProcessChannel>(processChannel));
    } else if (command == QLatin1String(Protocol::QProcessWrite)) {
        QByteArray byteArray;
        data >> byteArray;
        sendData(socket, m_process->write(byteArray));
    } else if (command == QLatin1String(Protocol::QProcessProcessChannelMode)) {
        sendData(socket, static_cast<qint32> (m_process->processChannelMode()));
    } else if (command == QLatin1String(Protocol::QProcessSetProcessChannelMode)) {
        qint32 processChannel;
        data >> processChannel;
        m_process->setProcessChannelMode(static_cast<QProcess::ProcessChannelMode>(processChannel));
    }
#ifdef Q_OS_WIN
    else if (command == QLatin1String(Protocol::QProcessSetNativeArguments)) {
        QString arguments;
        data >> arguments;
        m_process->setNativeArguments(arguments);
    }
#endif
    else if (!command.isEmpty()) {
        qDebug() << "Unknown QProcess command:" << command;
    }
}

void RemoteServerConnection::handleQSettings(QIODevice *socket, const QString &command,
                                             QDataStream &data, PermissionSettings *settings)
{
    if (!settings)
        return;

    if (command == QLatin1String(Protocol::QSettingsAllKeys)) {
        sendData(socket, settings->allKeys());
    } else if (command == QLatin1String(Protocol::QSettingsBeginGroup)) {
        QString prefix;
        data >> prefix;
        settings->beginGroup(prefix);
    } else if (command == QLatin1String(Protocol::QSettingsBeginWriteArray)) {
        QString prefix;
        data >> prefix;
        qint32 size;
        data >> size;
        settings->beginWriteArray(prefix, size);
    } else if (command == QLatin1String(Protocol::QSettingsBeginReadArray)) {
        QString prefix;
        data >> prefix;
        sendData(socket, settings->beginReadArray(prefix));
    } else if (command == QLatin1String(Protocol::QSettingsChildGroups)) {
        sendData(socket, settings->childGroups());
    } else if (command == QLatin1String(Protocol::QSettingsChildKeys)) {
        sendData(socket, settings->childKeys());
    } else if (command == QLatin1String(Protocol::QSettingsClear)) {
        settings->clear();
    } else if (command == QLatin1String(Protocol::QSettingsContains)) {
        QString key;
        data >> key;
        sendData(socket, settings->contains(key));
    } else if (command == QLatin1String(Protocol::QSettingsEndArray)) {
        settings->endArray();
    } else if (command == QLatin1String(Protocol::QSettingsEndGroup)) {
        settings->endGroup();
    } else if (command == QLatin1String(Protocol::QSettingsFallbacksEnabled)) {
        sendData(socket, settings->fallbacksEnabled());
    } else if (command == QLatin1String(Protocol::QSettingsFileName)) {
        sendData(socket, settings->fileName());
    } else if (command == QLatin1String(Protocol::QSettingsGroup)) {
        sendData(socket, settings->group());
    } else if (command == QLatin1String(Protocol::QSettingsIsWritable)) {
        sendData(socket, settings->isWritable());
    } else if (command == QLatin1String(Protocol::QSettingsRemove)) {
        QString key;
        data >> key;
        settings->remove(key);
    } else if (command == QLatin1String(Protocol::QSettingsSetArrayIndex)) {
        qint32 i;
        data >> i;
        settings->setArrayIndex(i);
    } else if (command == QLatin1String(Protocol::QSettingsSetFallbacksEnabled)) {
        bool b;
        data >> b;
        settings->setFallbacksEnabled(b);
    } else if (command == QLatin1String(Protocol::QSettingsStatus)) {
        sendData(socket, settings->status());
    } else if (command == QLatin1String(Protocol::QSettingsSync)) {
        settings->sync();
    } else if (command == QLatin1String(Protocol::QSettingsSetValue)) {
        QString key;
        QVariant value;
        data >> key;
        data >> value;
        settings->setValue(key, value);
    } else if (command == QLatin1String(Protocol::QSettingsValue)) {
        QString key;
        QVariant defaultValue;
        data >> key;
        data >> defaultValue;
        sendData(socket, settings->value(key, defaultValue));
    } else if (command == QLatin1String(Protocol::QSettingsOrganizationName)) {
        sendData(socket, settings->organizationName());
    } else if (command == QLatin1String(Protocol::QSettingsApplicationName)) {
        sendData(socket, settings->applicationName());
    } else if (!command.isEmpty()) {
        qDebug() << "Unknown QSettings command:" << command;
    }
}

void RemoteServerConnection::handleQFSFileEngine(QIODevice *socket, const QString &command,
                                                 QDataStream &data)
{
    if (command == QLatin1String(Protocol::QAbstractFileEngineAtEnd)) {
        sendData(socket, m_engine->atEnd());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCaseSensitive)) {
        sendData(socket, m_engine->caseSensitive());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineClose)) {
        sendData(socket, m_engine->close());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCopy)) {
        QString newName;
        data >>newName;
        sendData(socket, m_engine->copy(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineEntryList)) {
        qint32 filters;
        QStringList filterNames;
        data >>filters;
        data >>filterNames;
        sendData(socket, m_engine->entryList(static_cast<QDir::Filters> (filters), filterNames));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineError)) {
        sendData(socket, static_cast<qint32> (m_engine->error()));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineErrorString)) {
        sendData(socket, m_engine->errorString());
    }
    else if (command == QLatin1String(Protocol::QAbstractFileEngineFileFlags)) {
        qint32 flags;
        data >>flags;
        flags = m_engine->fileFlags(static_cast<QAbstractFileEngine::FileFlags>(flags));
        sendData(socket, static_cast<qint32>(flags));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFileName)) {
        qint32 file;
        data >>file;
        sendData(socket, m_engine->fileName(static_cast<QAbstractFileEngine::FileName> (file)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFlush)) {
        sendData(socket, m_engine->flush());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineHandle)) {
        sendData(socket, m_engine->handle());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsRelativePath)) {
        sendData(socket, m_engine->isRelativePath());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsSequential)) {
        sendData(socket, m_engine->isSequential());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineLink)) {
        QString newName;
        data >>newName;
        sendData(socket, m_engine->link(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineMkdir)) {
        QString dirName;
        bool createParentDirectories;
        data >>dirName;
        data >>createParentDirectories;
        sendData(socket, m_engine->mkdir(dirName, createParentDirectories));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOpen)) {
        qint32 openMode;
        data >>openMode;
        sendData(socket, m_engine->open(static_cast<QIODevice::OpenMode> (openMode)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwner)) {
        qint32 owner;
        data >>owner;
        sendData(socket, m_engine->owner(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwnerId)) {
        qint32 owner;
        data >>owner;
        sendData(socket, m_engine->ownerId(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEnginePos)) {
        sendData(socket, m_engine->pos());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRead)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->read(byteArray.data(), maxlen);
        sendData(socket, qMakePair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineReadLine)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->readLine(byteArray.data(), maxlen);
        sendData(socket, qMakePair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRemove)) {
        sendData(socket, m_engine->remove());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRename)) {
        QString newName;
        data >>newName;
        sendData(socket, m_engine->rename(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRmdir)) {
        QString dirName;
        bool recurseParentDirectories;
        data >>dirName;
        data >>recurseParentDirectories;
        sendData(socket, m_engine->rmdir(dirName, recurseParentDirectories));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSeek)) {
        quint64 offset;
        data >>offset;
        sendData(socket, m_engine->seek(offset));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetFileName)) {
        QString fileName;
        data >>fileName;
        m_engine->setFileName(fileName);
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetPermissions)) {
        uint perms;
        data >>perms;
        sendData(socket, m_engine->setPermissions(perms));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetSize)) {
        qint64 size;
        data >>size;
        sendData(socket, m_engine->setSize(size));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSize)) {
        sendData(socket, m_engine->size());
    } else if ((command == QLatin1String(Protocol::QAbstractFileEngineSupportsExtension))
        || (command == QLatin1String(Protocol::QAbstractFileEngineExtension))) {
            // Implemented client side.
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineWrite)) {
        QByteArray content;
        data >> content;
        sendData(socket, m_engine->write(content.data(), content.size()));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSyncToDisk)) {
        sendData(socket, m_engine->syncToDisk());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRenameOverwrite)) {
        QString newFilename;
        data >> newFilename;
        sendData(socket, m_engine->renameOverwrite(newFilename));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFileTime)) {
        qint32 filetime;
        data >> filetime;
        sendData(socket, m_engine->fileTime(static_cast<QAbstractFileEngine::FileTime> (filetime)));
    } else if (!command.isEmpty()) {
        qDebug() << "Unknown QAbstractFileEngine command:" << command;
    }
}

} // namespace QInstaller
