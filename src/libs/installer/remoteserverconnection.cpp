/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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
#include "globals.h"
#ifdef IFW_LIBARCHIVE
#include "libarchivearchive.h"
#endif

#include <QCoreApplication>
#include <QDataStream>
#include <QLocalSocket>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::QProcessSignalReceiver
    \internal
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteServerReply
    \internal
    \brief The RemoteServerReply class is used for sending a reply to a local socket.

           The class ensures exactly one reply is sent in the lifetime of an object
           instantiated from this class. If the calling client has not explicitly sent
           a reply, a default reply is automatically sent on destruction.
*/

/*!
    Constructs reply object for \a socket.
*/
RemoteServerReply::RemoteServerReply(QLocalSocket *socket)
    : m_socket(socket)
    , m_sent(false)
{}

/*!
    Destroys the object and sends the default reply, if
    no other replies have been sent.
*/
RemoteServerReply::~RemoteServerReply()
{
    send(QString::fromLatin1(Protocol::DefaultReply));
}

/*!
    \fn template <typename T> QInstaller::RemoteServerReply::send(const T &data)

    Sends a reply packet with \a¸ data to the socket. If a reply
    has been already sent, this function does not send another.
*/
template <typename T>
void RemoteServerReply::send(const T &data)
{
    if (m_sent || m_socket->state() != QLocalSocket::ConnectedState)
        return;

    QByteArray result;
    QDataStream returnStream(&result, QIODevice::WriteOnly);
    returnStream << data;

    sendPacket(m_socket, Protocol::Reply, result);
    m_socket->flush();
    m_sent = true;
}

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteServerConnection
    \internal
*/

RemoteServerConnection::RemoteServerConnection(qintptr socketDescriptor, const QString &key,
                                               QObject *parent)
    : QThread(parent)
    , m_socketDescriptor(socketDescriptor)
    , m_authorizationKey(key)
    , m_process(nullptr)
    , m_engine(nullptr)
    , m_archive(nullptr)
    , m_processSignalReceiver(nullptr)
    , m_archiveSignalReceiver(nullptr)
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
            qApp->processEvents();
            continue;
        }

        const QString command = QString::fromLatin1(cmd);
        QBuffer buf;
        buf.setBuffer(&data);
        buf.open(QIODevice::ReadOnly);
        QDataStream stream;
        stream.setDevice(&buf);
        StreamChecker streamChecker(&stream);

        RemoteServerReply reply(&socket);

        if (authorized && command == QLatin1String(Protocol::Shutdown)) {
            authorized = false;
            reply.send(true);
            socket.close();
            emit shutdownRequested();
            return;
        } else if (command == QLatin1String(Protocol::Authorize)) {
            QString key;
            stream >> key;
            reply.send(authorized = (key == m_authorizationKey));
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
                    m_process.reset(new QProcess);
                    m_processSignalReceiver = new QProcessSignalReceiver(m_process.get());
                } else if (type == QLatin1String(Protocol::QAbstractFileEngine)) {
                    m_engine.reset(new QFSFileEngine);
                } else if (type == QLatin1String(Protocol::AbstractArchive)) {
#ifdef IFW_LIBARCHIVE
                    m_archive.reset(new LibArchiveArchive);
                    m_archiveSignalReceiver = new AbstractArchiveSignalReceiver(
                        static_cast<LibArchiveArchive *>(m_archive.get()));
#else
                    Q_ASSERT_X(false, Q_FUNC_INFO, "No compatible archive handler exists for protocol.");
#endif
                } else {
                    qCDebug(QInstaller::lcServer) << "Unknown type for Create command:" << type;
                }
                continue;
            }

            if (command == QLatin1String(Protocol::GetQProcessSignals)) {
                if (m_processSignalReceiver) {
                    QMutexLocker _(&m_processSignalReceiver->m_lock);
                    reply.send(m_processSignalReceiver->m_receivedSignals);
                    m_processSignalReceiver->m_receivedSignals.clear();
                }
                continue;
            } else if (command == QLatin1String(Protocol::GetAbstractArchiveSignals)) {
#ifdef IFW_LIBARCHIVE
                if (m_archiveSignalReceiver) {
                    QMutexLocker _(&m_archiveSignalReceiver->m_lock);
                    reply.send(m_archiveSignalReceiver->m_receivedSignals);
                    m_archiveSignalReceiver->m_receivedSignals.clear();
                }
                continue;
#else
                Q_ASSERT_X(false, Q_FUNC_INFO, "No compatible archive handler exists for protocol.");
#endif
            }

            if (command.startsWith(QLatin1String(Protocol::QProcess))) {
                handleQProcess(&reply, command, stream);
            } else if (command.startsWith(QLatin1String(Protocol::QSettings))) {
                handleQSettings(&reply, command, stream, settings.data());
            } else if (command.startsWith(QLatin1String(Protocol::QAbstractFileEngine))) {
                handleQFSFileEngine(&reply, command, stream);
            } else if (command.startsWith(QLatin1String(Protocol::AbstractArchive))) {
                handleArchive(&reply, command, stream);
            } else {
                qCDebug(QInstaller::lcServer) << "Unknown command:" << command;
            }
        } else {
            // authorization failed, connection not wanted
            socket.close();
            qCDebug(QInstaller::lcServer) << "Authorization failed.";
            return;
        }
    }
}

void RemoteServerConnection::handleQProcess(RemoteServerReply *reply, const QString &command, QDataStream &data)
{
    if (command == QLatin1String(Protocol::QProcessCloseWriteChannel)) {
        m_process->closeWriteChannel();
    } else if (command == QLatin1String(Protocol::QProcessExitCode)) {
        reply->send(m_process->exitCode());
    } else if (command == QLatin1String(Protocol::QProcessExitStatus)) {
        reply->send(static_cast<qint32> (m_process->exitStatus()));
    } else if (command == QLatin1String(Protocol::QProcessKill)) {
        m_process->kill();
    } else if (command == QLatin1String(Protocol::QProcessReadAll)) {
        reply->send(m_process->readAll());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardOutput)) {
        reply->send(m_process->readAllStandardOutput());
    } else if (command == QLatin1String(Protocol::QProcessReadAllStandardError)) {
        reply->send(m_process->readAllStandardError());
    } else if (command == QLatin1String(Protocol::QProcessStartDetached)) {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        data >> program;
        data >> arguments;
        data >> workingDirectory;

        qint64 pid = -1;
        bool success = QInstaller::startDetached(program, arguments, workingDirectory, &pid);
        reply->send(QPair<bool, qint64>(success, pid));
    } else if (command == QLatin1String(Protocol::QProcessStartDetached2)) {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        data >> program;
        data >> arguments;
        data >> workingDirectory;

        qint64 pid = -1;
        bool success = QProcess::startDetached(program, arguments, workingDirectory, &pid);
        reply->send(QPair<bool, qint64>(success, pid));
    } else if (command == QLatin1String(Protocol::QProcessSetWorkingDirectory)) {
        QString dir;
        data >> dir;
        m_process->setWorkingDirectory(dir);
    } else if (command == QLatin1String(Protocol::QProcessSetEnvironment)) {
        QStringList env;
        data >> env;
        m_process->setEnvironment(env);
    } else if (command == QLatin1String(Protocol::QProcessEnvironment)) {
        reply->send(m_process->environment());
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
        m_process->start(program, {}, static_cast<QIODevice::OpenMode> (mode));
    } else if (command == QLatin1String(Protocol::QProcessState)) {
        reply->send(static_cast<qint32> (m_process->state()));
    } else if (command == QLatin1String(Protocol::QProcessTerminate)) {
        m_process->terminate();
    } else if (command == QLatin1String(Protocol::QProcessWaitForFinished)) {
        qint32 msecs;
        data >> msecs;
        reply->send(m_process->waitForFinished(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWaitForStarted)) {
        qint32 msecs;
        data >> msecs;
        reply->send(m_process->waitForStarted(msecs));
    } else if (command == QLatin1String(Protocol::QProcessWorkingDirectory)) {
        reply->send(m_process->workingDirectory());
    } else if (command == QLatin1String(Protocol::QProcessErrorString)) {
        reply->send(m_process->errorString());
    } else if (command == QLatin1String(Protocol::QProcessReadChannel)) {
        reply->send(static_cast<qint32> (m_process->readChannel()));
    } else if (command == QLatin1String(Protocol::QProcessSetReadChannel)) {
        qint32 processChannel;
        data >> processChannel;
        m_process->setReadChannel(static_cast<QProcess::ProcessChannel>(processChannel));
    } else if (command == QLatin1String(Protocol::QProcessWrite)) {
        QByteArray byteArray;
        data >> byteArray;
        reply->send(m_process->write(byteArray));
    } else if (command == QLatin1String(Protocol::QProcessProcessChannelMode)) {
        reply->send(static_cast<qint32> (m_process->processChannelMode()));
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
        qCDebug(QInstaller::lcServer) << "Unknown QProcess command:" << command;
    }
}

void RemoteServerConnection::handleQSettings(RemoteServerReply *reply, const QString &command,
                                             QDataStream &data, PermissionSettings *settings)
{
    if (!settings)
        return;

    if (command == QLatin1String(Protocol::QSettingsAllKeys)) {
        reply->send(settings->allKeys());
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
        reply->send(settings->beginReadArray(prefix));
    } else if (command == QLatin1String(Protocol::QSettingsChildGroups)) {
        reply->send(settings->childGroups());
    } else if (command == QLatin1String(Protocol::QSettingsChildKeys)) {
        reply->send(settings->childKeys());
    } else if (command == QLatin1String(Protocol::QSettingsClear)) {
        settings->clear();
    } else if (command == QLatin1String(Protocol::QSettingsContains)) {
        QString key;
        data >> key;
        reply->send(settings->contains(key));
    } else if (command == QLatin1String(Protocol::QSettingsEndArray)) {
        settings->endArray();
    } else if (command == QLatin1String(Protocol::QSettingsEndGroup)) {
        settings->endGroup();
    } else if (command == QLatin1String(Protocol::QSettingsFallbacksEnabled)) {
        reply->send(settings->fallbacksEnabled());
    } else if (command == QLatin1String(Protocol::QSettingsFileName)) {
        reply->send(settings->fileName());
    } else if (command == QLatin1String(Protocol::QSettingsGroup)) {
        reply->send(settings->group());
    } else if (command == QLatin1String(Protocol::QSettingsIsWritable)) {
        reply->send(settings->isWritable());
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
        reply->send(settings->status());
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
        if (!data.atEnd())
            data >> defaultValue;
        reply->send(settings->value(key, defaultValue));
    } else if (command == QLatin1String(Protocol::QSettingsOrganizationName)) {
        reply->send(settings->organizationName());
    } else if (command == QLatin1String(Protocol::QSettingsApplicationName)) {
        reply->send(settings->applicationName());
    } else if (!command.isEmpty()) {
        qCDebug(QInstaller::lcServer) << "Unknown QSettings command:" << command;
    }
}

void RemoteServerConnection::handleQFSFileEngine(RemoteServerReply *reply, const QString &command,
                                                 QDataStream &data)
{
    if (command == QLatin1String(Protocol::QAbstractFileEngineAtEnd)) {
        reply->send(m_engine->atEnd());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCaseSensitive)) {
        reply->send(m_engine->caseSensitive());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineClose)) {
        reply->send(m_engine->close());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineCopy)) {
        QString newName;
        data >>newName;
#ifdef Q_OS_LINUX
        // QFileSystemEngine::copyFile() is currently unimplemented on Linux,
        // copy using QFile instead of directly with QFSFileEngine.
        QFile file(m_engine->fileName(QAbstractFileEngine::AbsoluteName));
        reply->send(file.copy(newName));
#else
        reply->send(m_engine->copy(newName));
#endif
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineEntryList)) {
        qint32 filters;
        QStringList filterNames;
        data >>filters;
        data >>filterNames;
        reply->send(m_engine->entryList(static_cast<QDir::Filters> (filters), filterNames));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineError)) {
        reply->send(static_cast<qint32> (m_engine->error()));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineErrorString)) {
        reply->send(m_engine->errorString());
    }
    else if (command == QLatin1String(Protocol::QAbstractFileEngineFileFlags)) {
        qint32 flags;
        data >>flags;
        flags = m_engine->fileFlags(static_cast<QAbstractFileEngine::FileFlags>(flags));
        reply->send(static_cast<qint32>(flags));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFileName)) {
        qint32 file;
        data >>file;
        reply->send(m_engine->fileName(static_cast<QAbstractFileEngine::FileName> (file)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFlush)) {
        reply->send(m_engine->flush());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineHandle)) {
        reply->send(m_engine->handle());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsRelativePath)) {
        reply->send(m_engine->isRelativePath());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineIsSequential)) {
        reply->send(m_engine->isSequential());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineLink)) {
        QString newName;
        data >>newName;
        reply->send(m_engine->link(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineMkdir)) {
        QString dirName;
        bool createParentDirectories;
        data >>dirName;
        data >>createParentDirectories;
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
        reply->send(m_engine->mkdir(dirName, createParentDirectories));
#else
        reply->send(m_engine->mkdir(dirName, createParentDirectories, std::nullopt));
#endif

    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOpen)) {
        qint32 openMode;
        data >>openMode;
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
        reply->send(m_engine->open(static_cast<QIODevice::OpenMode> (openMode)));
#else
        reply->send(m_engine->open(static_cast<QIODevice::OpenMode> (openMode), std::nullopt));
#endif
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwner)) {
        qint32 owner;
        data >>owner;
        reply->send(m_engine->owner(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineOwnerId)) {
        qint32 owner;
        data >>owner;
        reply->send(m_engine->ownerId(static_cast<QAbstractFileEngine::FileOwner> (owner)));
    } else if (command == QLatin1String(Protocol::QAbstractFileEnginePos)) {
        reply->send(m_engine->pos());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRead)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->read(byteArray.data(), maxlen);
        reply->send(QPair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineReadLine)) {
        qint64 maxlen;
        data >> maxlen;
        QByteArray byteArray(maxlen, '\0');
        const qint64 r = m_engine->readLine(byteArray.data(), maxlen);
        reply->send(QPair<qint64, QByteArray>(r, byteArray));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRemove)) {
        reply->send(m_engine->remove());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRename)) {
        QString newName;
        data >>newName;
        reply->send(m_engine->rename(newName));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRmdir)) {
        QString dirName;
        bool recurseParentDirectories;
        data >>dirName;
        data >>recurseParentDirectories;
        reply->send(m_engine->rmdir(dirName, recurseParentDirectories));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSeek)) {
        quint64 offset;
        data >>offset;
        reply->send(m_engine->seek(offset));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetFileName)) {
        QString fileName;
        data >>fileName;
        m_engine->setFileName(fileName);
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetPermissions)) {
        uint perms;
        data >>perms;
        reply->send(m_engine->setPermissions(perms));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSetSize)) {
        qint64 size;
        data >>size;
        reply->send(m_engine->setSize(size));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSize)) {
        reply->send(m_engine->size());
    } else if ((command == QLatin1String(Protocol::QAbstractFileEngineSupportsExtension))
        || (command == QLatin1String(Protocol::QAbstractFileEngineExtension))) {
            // Implemented client side.
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineWrite)) {
        QByteArray content;
        data >> content;
        reply->send(m_engine->write(content.data(), content.size()));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineSyncToDisk)) {
        reply->send(m_engine->syncToDisk());
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineRenameOverwrite)) {
        QString newFilename;
        data >> newFilename;
        reply->send(m_engine->renameOverwrite(newFilename));
    } else if (command == QLatin1String(Protocol::QAbstractFileEngineFileTime)) {
        qint32 filetime;
        data >> filetime;
        reply->send(m_engine->fileTime(static_cast<QAbstractFileEngine::FileTime> (filetime)));
    } else if (!command.isEmpty()) {
        qCDebug(QInstaller::lcServer) << "Unknown QAbstractFileEngine command:" << command;
    }
}

void RemoteServerConnection::handleArchive(RemoteServerReply *reply, const QString &command, QDataStream &data)
{
#ifdef IFW_LIBARCHIVE
    LibArchiveArchive *archive = static_cast<LibArchiveArchive *>(m_archive.get());
    if (command == QLatin1String(Protocol::AbstractArchiveOpen)) {
        qint32 openMode;
        data >> openMode;
        reply->send(archive->open(static_cast<QIODevice::OpenMode>(openMode)));
    } else if (command == QLatin1String(Protocol::AbstractArchiveClose)) {
        archive->close();
    } else if (command == QLatin1String(Protocol::AbstractArchiveSetFilename)) {
        QString fileName;
        data >> fileName;
        archive->setFilename(fileName);
    } else if (command == QLatin1String(Protocol::AbstractArchiveErrorString)) {
        reply->send(archive->errorString());
    } else if (command == QLatin1String(Protocol::AbstractArchiveExtract)) {
        QString dirPath;
        quint64 total;
        data >> dirPath;
        data >> total;
        archive->workerExtract(dirPath, total);
    } else if (command == QLatin1String(Protocol::AbstractArchiveCreate)) {
        QStringList entries;
        data >> entries;
        reply->send(archive->create(entries));
    } else if (command == QLatin1String(Protocol::AbstractArchiveList)) {
        reply->send(archive->list());
    } else if (command == QLatin1String(Protocol::AbstractArchiveIsSupported)) {
        reply->send(archive->isSupported());
    } else if (command == QLatin1String(Protocol::AbstractArchiveSetCompressionLevel)) {
        qint32 level;
        data >> level;
        archive->setCompressionLevel(static_cast<AbstractArchive::CompressionLevel>(level));
    } else if (command == QLatin1String(Protocol::AbstractArchiveAddDataBlock)) {
        QByteArray buff;
        data >> buff;
        archive->workerAddDataBlock(buff);
    } else if (command == QLatin1String(Protocol::AbstractArchiveSetClientDataAtEnd)) {
        archive->workerSetDataAtEnd();
    } else if (command == QLatin1String(Protocol::AbstractArchiveSetFilePosition)) {
        qint64 pos;
        data >> pos;
        archive->workerSetFilePosition(pos);
    } else if (command == QLatin1String(Protocol::AbstractArchiveWorkerStatus)) {
        reply->send(static_cast<qint32>(archive->workerStatus()));
    } else if (command == QLatin1String(Protocol::AbstractArchiveCancel)) {
        archive->workerCancel();
    } else if (!command.isEmpty()) {
        qCDebug(QInstaller::lcServer) << "Unknown AbstractArchive command:" << command;
    }
#else
    Q_ASSERT_X(false, Q_FUNC_INFO, "No compatible archive handler exists for protocol.");
#endif
}

} // namespace QInstaller
