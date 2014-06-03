/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

#include "remoteserverconnection.h"

#include "errors.h"
#include "protocol.h"
#include "remoteserver.h"
#include "remoteserverconnection_p.h"
#include "utils.h"

#include <QSettings>
#include <QTcpSocket>

namespace QInstaller {

RemoteServerConnection::RemoteServerConnection(qintptr socketDescriptor, RemoteServer *parent)
    : m_socketDescriptor(socketDescriptor)
    , m_process(0)
    , m_settings(0)
    , m_engine(0)
    , m_server(parent)
    , m_signalReceiver(0)
{
}

void RemoteServerConnection::run()
{
    QTcpSocket socket;
    socket.setSocketDescriptor(m_socketDescriptor);

    QDataStream stream;
    stream.setDevice(&socket);

    bool authorized = false;
    while (socket.state() == QAbstractSocket::ConnectedState) {
        // Use a polling approach here to kill the thread as soon as the connections
        // closes. This seems to be related to the fact that the keep alive thread connects
        // every second and immediately throws away the socket and therefor the connection.
        if (!socket.bytesAvailable() && !socket.waitForReadyRead(250))
            continue;

        QString command;
        stream >> command;

        if (authorized && command == QLatin1String(Protocol::Shutdown)) {
            // this is a graceful shutdown
            socket.close();
            if (m_server)
                m_server->deleteLater();
            return;
        } else if (command == QLatin1String(Protocol::Authorize)) {
            QString key;
            stream >> key;
            sendData(stream, (authorized = (m_server && (key == m_server->authorizationKey()))));
            if (!authorized)
                socket.close();
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

                    if (m_settings)
                        m_settings->deleteLater();
                    if (fileName.toString().isEmpty()) {
                        m_settings = new QSettings(QSettings::Format(format.toInt()),
                            QSettings::Scope(scope.toInt()), organization.toString(), application
                            .toString());
                    } else {
                        m_settings = new QSettings(fileName.toString(), QSettings::NativeFormat);
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
                    m_settings->deleteLater();
                    m_settings = 0;
                } else if (command == QLatin1String(Protocol::QProcess)) {
                    m_signalReceiver->m_receivedSignals.clear();
                    m_process->deleteLater();
                    m_process = 0;
                } else if (command == QLatin1String(Protocol::QAbstractFileEngine)) {
                    delete m_engine;
                    m_engine = 0;
                }
                continue;
            }

            if (command == QLatin1String(Protocol::GetQProcessSignals)) {
                if (m_signalReceiver) {
                    QMutexLocker _(&m_signalReceiver->m_lock);
                    sendData(stream, m_signalReceiver->m_receivedSignals);
                    m_signalReceiver->m_receivedSignals.clear();
                }
                continue;
            }

            if (command.startsWith(QLatin1String(Protocol::QProcess))) {
                handleQProcess(command, stream);
            } else if (command.startsWith(QLatin1String(Protocol::QSettings))) {
                handleQSettings(command, stream);
            } else if (command.startsWith(QLatin1String(Protocol::QAbstractFileEngine))) {
                handleQFSFileEngine(command, stream);
            } else {
                qDebug() << "Unknown command:" << command;
            }
        } else {
            // authorization failed, connection not wanted
            socket.close();
            qDebug() << "Unknown command:" << command;
            return;
        }
    }
}

template <typename T>
void RemoteServerConnection::sendData(QDataStream &stream, const T &data)
{
    QByteArray result;
    QDataStream returnStream(&result, QIODevice::WriteOnly);
    returnStream << data;

    stream << static_cast<quint32> (result.size());
    if (!result.isEmpty())
        stream.writeRawData(result.data(), result.size());
}

void RemoteServerConnection::handleQProcess(const QString &command, QDataStream &stream)
{
    quint32 size;
    stream >> size;
    while (stream.device()->bytesAvailable() < size) {
        if (!stream.device()->waitForReadyRead(30000)) {
            throw Error(tr("Could not read all data after sending command: %1. "
                "Bytes expected: %2, Bytes received: %3. Error: %3").arg(command).arg(size)
                .arg(stream.device()->bytesAvailable()).arg(stream.device()->errorString()));
        }
    }

    QByteArray ba;
    stream >> ba;
    QDataStream data(ba);

    if (command == QLatin1String(Protocol::QProcessCloseWriteChannel)) {
        m_process->closeWriteChannel();
    } else if (command == QLatin1String(Protocol::QProcessExitCode)) {
        sendData(stream, m_process->exitCode());
    } else if (command == QLatin1String(Protocol::QProcessExitStatus)) {
        sendData(stream, static_cast<int> (m_process->exitStatus()));
    } else if (command == QLatin1String(Protocol::QProcessKill)) {
        m_process->kill();
    } else if (command == QLatin1String(Protocol::QProcessReadAll)) {
        sendData(stream, m_process->readAll());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardOutput)) {
        sendData(stream, m_process->readAllStandardOutput());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardError)) {
        sendData(stream, m_process->readAllStandardError());
    } else if (command == QLatin1String(Protocol::QProcessStartDetached)) {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        data >> program;
        data >> arguments;
        data >> workingDirectory;

        qint64 pid = -1;
        bool success = QInstaller::startDetached(program, arguments, workingDirectory, &pid);
        sendData(stream, qMakePair< bool, qint64>(success, pid));
    } else if (command == QLatin1String(Protocol::QProcessSetWorkingDirectory)) {
        QString dir;
        data >> dir;
        m_process->setWorkingDirectory(dir);
    } else if (command == QLatin1String(Protocol::QProcessSetEnvironment)) {
        QStringList env;
        data >> env;
        m_process->setEnvironment(env);
    } else if (command == QLatin1String(Protocol::QProcessEnvironment)) {
        sendData(stream, m_process->environment());
    } else if (command == QLatin1String(Protocol::QProcessStart3Arg)) {
        QString program;
        QStringList arguments;
        int mode;
        data >> program;
        data >> arguments;
        data >> mode;
        m_process->start(program, arguments, static_cast<QIODevice::OpenMode> (mode));
    } else if (command == QLatin1String(Protocol::QProcessStart2Arg)) {
        QString program;
        int mode;
        data >> program;
        data >> mode;
        m_process->start(program, static_cast<QIODevice::OpenMode> (mode));
    } else if (command == QLatin1String(Protocol::QProcessState)) {
        sendData(stream, static_cast<int> (m_process->state()));
    } else if (command == QLatin1String(Protocol::QProcessTerminate)) {
        m_process->terminate();
    } else if (command == QLatin1String(Protocol::QProcessWaitForFinished)) {
        int msecs;
        data >> msecs;
        sendData(stream, m_process->waitForFinished(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWaitForStarted)) {
        int msecs;
        data >> msecs;
        sendData(stream, m_process->waitForStarted(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWorkingDirectory)) {
        sendData(stream, m_process->workingDirectory());
    } else if (command == QLatin1String(Protocol::QProcessErrorString)) {
        sendData(stream, m_process->errorString());
    } else if (command == QLatin1String(Protocol::QProcessReadChannel)) {
        sendData(stream, static_cast<int> (m_process->readChannel()));
    } else if (command == QLatin1String(Protocol::QProcessSetReadChannel)) {
        int processChannel;
        data >> processChannel;
        m_process->setReadChannel(static_cast<QProcess::ProcessChannel>(processChannel));
    } else if (command == QLatin1String(Protocol::QProcessWrite)) {
        QByteArray byteArray;
        data >> byteArray;
        sendData(stream, m_process->write(byteArray));
    } else if (command == QLatin1String(Protocol::QProcessProcessChannelMode)) {
        sendData(stream, static_cast<int> (m_process->processChannelMode()));
    } else if (command == QLatin1String(Protocol::QProcessSetProcessChannelMode)) {
        int processChannel;
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

void RemoteServerConnection::handleQSettings(const QString &command, QDataStream &stream)
{
    quint32 size;
    stream >> size;
    while (stream.device()->bytesAvailable() < size) {
        if (!stream.device()->waitForReadyRead(30000)) {
            throw Error(tr("Could not read all data after sending command: %1. "
                "Bytes expected: %2, Bytes received: %3. Error: %3").arg(command).arg(size)
                .arg(stream.device()->bytesAvailable()).arg(stream.device()->errorString()));
        }
    }

    QByteArray ba;
    stream >> ba;
    QDataStream data(ba);

    if (command == QLatin1String(Protocol::QSettingsAllKeys)) {
        sendData(stream, m_settings->allKeys());
    } else if (command == QLatin1String(Protocol::QSettingsBeginGroup)) {
        QString prefix;
        data >> prefix;
        m_settings->beginGroup(prefix);
    } else if (command == QLatin1String(Protocol::QSettingsBeginWriteArray)) {
        QString prefix;
        data >> prefix;
        int size;
        data >> size;
        m_settings->beginWriteArray(prefix, size);
    } else if (command == QLatin1String(Protocol::QSettingsBeginReadArray)) {
        QString prefix;
        data >> prefix;
        sendData(stream, m_settings->beginReadArray(prefix));
    } else if (command == QLatin1String(Protocol::QSettingsChildGroups)) {
        sendData(stream, m_settings->childGroups());
    } else if (command == QLatin1String(Protocol::QSettingsChildKeys)) {
        sendData(stream, m_settings->childKeys());
    } else if (command == QLatin1String(Protocol::QSettingsClear)) {
        m_settings->clear();
    } else if (command == QLatin1String(Protocol::QSettingsContains)) {
        QString key;
        data >> key;
        sendData(stream, m_settings->contains(key));
    } else if (command == QLatin1String(Protocol::QSettingsEndArray)) {
        m_settings->endArray();
    } else if (command == QLatin1String(Protocol::QSettingsEndGroup)) {
        m_settings->endGroup();
    } else if (command == QLatin1String(Protocol::QSettingsFallbacksEnabled)) {
        sendData(stream, m_settings->fallbacksEnabled());
    } else if (command == QLatin1String(Protocol::QSettingsFileName)) {
        sendData(stream, m_settings->fileName());
    } else if (command == QLatin1String(Protocol::QSettingsGroup)) {
        sendData(stream, m_settings->group());
    } else if (command == QLatin1String(Protocol::QSettingsIsWritable)) {
        sendData(stream, m_settings->isWritable());
    } else if (command == QLatin1String(Protocol::QSettingsRemove)) {
        QString key;
        data >> key;
        m_settings->remove(key);
    } else if (command == QLatin1String(Protocol::QSettingsSetArrayIndex)) {
        int i;
        data >> i;
        m_settings->setArrayIndex(i);
    } else if (command == QLatin1String(Protocol::QSettingsSetFallbacksEnabled)) {
        bool b;
        data >> b;
        m_settings->setFallbacksEnabled(b);
    } else if (command == QLatin1String(Protocol::QSettingsStatus)) {
        sendData(stream, m_settings->status());
    } else if (command == QLatin1String(Protocol::QSettingsSync)) {
        m_settings->sync();
    } else if (command == QLatin1String(Protocol::QSettingsSetValue)) {
        QString key;
        QVariant value;
        data >> key;
        data >> value;
        m_settings->setValue(key, value);
    } else if (command == QLatin1String(Protocol::QSettingsValue)) {
        QString key;
        QVariant defaultValue;
        data >> key;
        data >> defaultValue;
        sendData(stream, m_settings->value(key, defaultValue));
    } else if (command == QLatin1String(Protocol::QSettingsOrganizationName)) {
        sendData(stream, m_settings->organizationName());
    } else if (command == QLatin1String(Protocol::QSettingsApplicationName)) {
        sendData(stream, m_settings->applicationName());
    } else if (!command.isEmpty()) {
        qDebug() << "Unknown QSettings command:" << command;
    }
}

void RemoteServerConnection::handleQFSFileEngine(const QString &command, QDataStream &stream)
{
    quint32 size;
    stream >> size;
    while (stream.device()->bytesAvailable() < size) {
        if (!stream.device()->waitForReadyRead(30000)) {
            throw Error(tr("Could not read all data after sending command: %1. "
                "Bytes expected: %2, Bytes received: %3. Error: %3").arg(command).arg(size)
                .arg(stream.device()->bytesAvailable()).arg(stream.device()->errorString()));
        }
    }

    QByteArray ba;
    stream >> ba;
    QDataStream data(ba);

    if (command == QLatin1String(Protocol::QAbstractFileEngineAtEnd)) {
        sendData(stream, m_engine->atEnd());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCaseSensitive)) {
        sendData(stream, m_engine->caseSensitive());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineClose)) {
        sendData(stream, m_engine->close());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCopy)) {
        QString newName;
        data >>newName;
        sendData(stream, m_engine->copy(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineEntryList)) {
        int filters;
        QStringList filterNames;
        data >>filters;
        data >>filterNames;
        sendData(stream, m_engine->entryList(static_cast<QDir::Filters> (filters), filterNames));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineError)) {
        sendData(stream, static_cast<int> (m_engine->error()));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineErrorString)) {
        sendData(stream, m_engine->errorString());
    }
    else if (command == QLatin1String(Protocol::QAbstractFileEngineFileFlags)) {
        int flags;
        data >>flags;
        sendData(stream,
            static_cast<int>(m_engine->fileFlags(static_cast<QAbstractFileEngine::FileFlags>(flags))));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFileName)) {
        int file;
        data >>file;
        sendData(stream, m_engine->fileName(static_cast<QAbstractFileEngine::FileName> (file)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFlush)) {
        sendData(stream, m_engine->flush());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineHandle)) {
        sendData(stream, m_engine->handle());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsRelativePath)) {
        sendData(stream, m_engine->isRelativePath());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsSequential)) {
        sendData(stream, m_engine->isSequential());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineLink)) {
        QString newName;
        data >>newName;
        sendData(stream, m_engine->link(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineMkdir)) {
        QString dirName;
        bool createParentDirectories;
        data >>dirName;
        data >>createParentDirectories;
        sendData(stream, m_engine->mkdir(dirName, createParentDirectories));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOpen)) {
        int openMode;
        data >>openMode;
        sendData(stream, m_engine->open(static_cast<QIODevice::OpenMode> (openMode)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwner)) {
        int owner;
        data >>owner;
        sendData(stream, m_engine->owner(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwnerId)) {
        int owner;
        data >>owner;
        sendData(stream, m_engine->ownerId(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEnginePos)) {
        sendData(stream, m_engine->pos());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRead)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->read(byteArray.data(), maxlen);
        sendData(stream, qMakePair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineReadLine)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->readLine(byteArray.data(), maxlen);
        sendData(stream, qMakePair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRemove)) {
        sendData(stream, m_engine->remove());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRename)) {
        QString newName;
        data >>newName;
        sendData(stream, m_engine->rename(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRmdir)) {
        QString dirName;
        bool recurseParentDirectories;
        data >>dirName;
        data >>recurseParentDirectories;
        sendData(stream, m_engine->rmdir(dirName, recurseParentDirectories));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSeek)) {
        quint64 offset;
        data >>offset;
        sendData(stream, m_engine->seek(offset));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetFileName)) {
        QString fileName;
        data >>fileName;
        m_engine->setFileName(fileName);
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetPermissions)) {
        uint perms;
        data >>perms;
        sendData(stream, m_engine->setPermissions(perms));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetSize)) {
        qint64 size;
        data >>size;
        sendData(stream, m_engine->setSize(size));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSize)) {
        sendData(stream, m_engine->size());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSupportsExtension)) {
            // Implemented client side
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineExtension)) {
        qint32 extension;
        data >>extension;
        sendData(stream, m_engine->extension(static_cast<QAbstractFileEngine::Extension> (extension)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineWrite)) {
        QByteArray content;
        data >> content;
        sendData(stream, m_engine->write(content.data(), content.size()));
    } else if (!command.isEmpty()) {
        qDebug() << "Unknown QAbstractFileEngine command:" << command;
    }
}

} // namespace QInstaller
