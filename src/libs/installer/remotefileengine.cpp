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

#include "remotefileengine.h"

#include "protocol.h"
#include "remoteclient.h"

#include <QRegularExpression>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::RemoteFileEngineHandler
    \internal
*/

// -- RemoteFileEngineHandler

QAbstractFileEngine* RemoteFileEngineHandler::create(const QString &fileName) const
{
    if (!RemoteClient::instance().isActive())
        return 0;

    static const QRegularExpression re(QLatin1String("^[a-z0-9]*://.*$"));
    if (re.match(fileName).hasMatch())  // stuff like installer://
        return 0;

    if (fileName.isEmpty() || fileName.startsWith(QLatin1String(":")))
        return 0; // empty filename or Qt resource

    std::unique_ptr<RemoteFileEngine> client(new RemoteFileEngine());
    client->setFileName(fileName);
    if (client->isConnectedToServer())
        return client.release();
    return 0;
}


// -- RemoteFileEngineIterator

class RemoteFileEngineIterator : public QAbstractFileEngineIterator
{
public:
    RemoteFileEngineIterator(QDir::Filters filters, const QStringList &nameFilters,
            const QStringList &files)
        : QAbstractFileEngineIterator(filters, nameFilters)
        , index(-1)
        , entries(files)
    {
    }

    QString next();
    bool hasNext() const;
    QString currentFileName() const;

private:
    qint32 index;
    QStringList entries;
};

/*
    Advances the iterator to the next entry, and returns the current file path of this new
    entry. If hasNext() returns \c false, the function does nothing and returns an empty QString.
*/
bool RemoteFileEngineIterator::hasNext() const
{
    return index < entries.size() - 1;
}

/*
    Returns \c true if there is at least one more entry in the directory, otherwise returns
    \c false.
*/
QString RemoteFileEngineIterator::next()
{
    if (!hasNext())
        return QString();
    ++index;
    return currentFilePath();
}

/*
    Returns the name of the current directory entry, excluding the path.
*/
QString RemoteFileEngineIterator::currentFileName() const
{
    return entries.at(index);
}


/*!
    \class QInstaller::RemoteFileEngine
    \inmodule QtInstallerFramework
    \internal
*/
RemoteFileEngine::RemoteFileEngine()
    : RemoteObject(QLatin1String(Protocol::QAbstractFileEngine))
{
}

RemoteFileEngine::~RemoteFileEngine()
{
}

/*!
*/
bool RemoteFileEngine::atEnd() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineAtEnd));
    return m_fileEngine.atEnd();
}

/*!
    \reimp
*/
QAbstractFileEngine::Iterator* RemoteFileEngine::beginEntryList(QDir::Filters filters,
    const QStringList &filterNames)
{
    if (connectToServer()) {
        QStringList entries = entryList(filters, filterNames);
        entries.removeAll(QString());
        return new RemoteFileEngineIterator(filters, filterNames, entries);
    }
    return m_fileEngine.beginEntryList(filters, filterNames);

}

QAbstractFileEngine::Iterator* RemoteFileEngine::endEntryList()
{
    if (connectToServer())
        return 0;   // right now all other implementations return 0 too
    return m_fileEngine.endEntryList();
}

/*!
    \reimp
*/
bool RemoteFileEngine::caseSensitive() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineCaseSensitive));
    return m_fileEngine.caseSensitive();
}

/*!
    \reimp
*/
bool RemoteFileEngine::close()
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineClose));
    return m_fileEngine.close();
}

/*!
    \reimp
*/
bool RemoteFileEngine::copy(const QString &newName)
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineCopy), newName);
    return m_fileEngine.copy(newName);
}

/*!
    \reimp
*/
QStringList RemoteFileEngine::entryList(QDir::Filters filters, const QStringList &filterNames) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<QStringList>
            (QString::fromLatin1(Protocol::QAbstractFileEngineEntryList),
            static_cast<qint32>(filters), filterNames);
    }
    return m_fileEngine.entryList(filters, filterNames);
}

/*!
*/
QFile::FileError RemoteFileEngine::error() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return static_cast<QFile::FileError>
            (callRemoteMethod<qint32>(QString::fromLatin1(Protocol::QAbstractFileEngineError)));
    }
    return m_fileEngine.error();
}

/*!
*/
QString RemoteFileEngine::errorString() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<QString>(QString::fromLatin1(Protocol::QAbstractFileEngineErrorString));
    return m_fileEngine.errorString();
}

/*!
    \reimp
*/
bool RemoteFileEngine::extension(Extension extension, const ExtensionOption *eo, ExtensionReturn *er)
{
    if (connectToServer())
        return false;
    return m_fileEngine.extension(extension, eo, er);
}

/*!
    \reimp
*/
QAbstractFileEngine::FileFlags RemoteFileEngine::fileFlags(FileFlags type) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return static_cast<QAbstractFileEngine::FileFlags>
            (callRemoteMethod<qint32>(QString::fromLatin1(Protocol::QAbstractFileEngineFileFlags),
            static_cast<qint32>(type)));
    }
    return m_fileEngine.fileFlags(type);
}

/*!
    \reimp
*/
QString RemoteFileEngine::fileName(FileName file) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<QString>(QString::fromLatin1(Protocol::QAbstractFileEngineFileName),
            static_cast<qint32>(file));
    }
    return m_fileEngine.fileName(file);
}

/*!
    \reimp
*/
bool RemoteFileEngine::flush()
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineFlush));
    return m_fileEngine.flush();
}

/*!
    \reimp
*/
int RemoteFileEngine::handle() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<qint32>(QString::fromLatin1(Protocol::QAbstractFileEngineHandle));
    return m_fileEngine.handle();
}

/*!
    \reimp
*/
bool RemoteFileEngine::isRelativePath() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineIsRelativePath));
    return m_fileEngine.isRelativePath();
}

/*!
    \reimp
*/
bool RemoteFileEngine::isSequential() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineIsSequential));
    return m_fileEngine.isSequential();
}

/*!
    \reimp
*/
bool RemoteFileEngine::link(const QString &newName)
{
    if (connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineLink),
            newName);
    }
    return m_fileEngine.link(newName);
}

/*!
    \reimp
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
bool RemoteFileEngine::mkdir(const QString &dirName, bool createParentDirectories) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineMkdir),
            dirName, createParentDirectories);
    }
    return m_fileEngine.mkdir(dirName, createParentDirectories);

}
#else
bool RemoteFileEngine::mkdir(const QString &dirName, bool createParentDirectories,
           std::optional<QFile::Permissions> permissions) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineMkdir),
            dirName, createParentDirectories);
    }
    return m_fileEngine.mkdir(dirName, createParentDirectories, permissions);
}
#endif

/*!
    \reimp
*/
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
bool RemoteFileEngine::open(QIODevice::OpenMode mode)
#else
bool RemoteFileEngine::open(QIODevice::OpenMode mode, std::optional<QFile::Permissions> permissions)
#endif
{
    if (connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineOpen),
            static_cast<qint32>(mode | QIODevice::Unbuffered));
    }
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    return m_fileEngine.open(mode | QIODevice::Unbuffered);
#else
    return m_fileEngine.open(mode | QIODevice::Unbuffered, permissions);
#endif
}

/*!
    \reimp
*/
QString RemoteFileEngine::owner(FileOwner owner) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<QString>(QString::fromLatin1(Protocol::QAbstractFileEngineOwner),
            static_cast<qint32>(owner));
    }
    return m_fileEngine.owner(owner);
}

/*!
    \reimp
*/
uint RemoteFileEngine::ownerId(FileOwner owner) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<quint32>(QString::fromLatin1(Protocol::QAbstractFileEngineOwnerId),
            static_cast<qint32>(owner));
    }
    return m_fileEngine.ownerId(owner);
}

/*!
    \reimp
*/
qint64 RemoteFileEngine::pos() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<qint64>(QString::fromLatin1(Protocol::QAbstractFileEnginePos));
    return m_fileEngine.pos();
}

/*!
    \reimp
*/
bool RemoteFileEngine::remove()
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineRemove));
    return m_fileEngine.remove();
}

/*!
    \reimp
*/
bool RemoteFileEngine::rename(const QString &newName)
{
    if (connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineRename),
            newName);
    }
    return m_fileEngine.rename(newName);
}

/*!
    \reimp
*/
bool RemoteFileEngine::rmdir(const QString &dirName, bool recurseParentDirectories) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineRmdir),
            dirName, recurseParentDirectories);
    }
    return m_fileEngine.rmdir(dirName, recurseParentDirectories);
}

/*!
    \reimp
*/
bool RemoteFileEngine::seek(qint64 offset)
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineSeek), offset);
    return m_fileEngine.seek(offset);
}

/*!
    \reimp
*/
void RemoteFileEngine::setFileName(const QString &fileName)
{
    if (connectToServer()) {
        callRemoteMethodDefaultReply(QString::fromLatin1(Protocol::QAbstractFileEngineSetFileName), fileName);
    }
    m_fileEngine.setFileName(fileName);
}

/*!
    \reimp
*/
bool RemoteFileEngine::setPermissions(uint perms)
{
    if (connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineSetPermissions),
            perms);
    }
    return m_fileEngine.setPermissions(perms);
}

/*!
    \reimp
*/
bool RemoteFileEngine::setSize(qint64 size)
{
    if (connectToServer()) {
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineSetSize),
            size);
    }
    return m_fileEngine.setSize(size);
}

/*!
    \reimp
*/
qint64 RemoteFileEngine::size() const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return callRemoteMethod<qint64>(QString::fromLatin1(Protocol::QAbstractFileEngineSize));
    return m_fileEngine.size();
}

/*!
    \reimp
*/
bool RemoteFileEngine::supportsExtension(Extension extension) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer())
        return false;
    return m_fileEngine.supportsExtension(extension);
}

/*!
    \reimp
*/
qint64 RemoteFileEngine::read(char *data, qint64 maxlen)
{
    if (connectToServer()) {
        QPair<qint64, QByteArray> result = callRemoteMethod<QPair<qint64, QByteArray> >
            (QString::fromLatin1(Protocol::QAbstractFileEngineRead), maxlen);

        if (result.first <= 0)
            return result.first;

        QDataStream dataStream(result.second);
        dataStream.readRawData(data, result.first);
        return result.first;
    }
    return m_fileEngine.read(data, maxlen);
}

/*!
    \reimp
*/
qint64 RemoteFileEngine::readLine(char *data, qint64 maxlen)
{
    if (connectToServer()) {
        QPair<qint64, QByteArray> result = callRemoteMethod<QPair<qint64, QByteArray> >
            (QString::fromLatin1(Protocol::QAbstractFileEngineReadLine), maxlen);

        if (result.first <= 0)
            return result.first;

        QDataStream dataStream(result.second);
        dataStream.readRawData(data, result.first);
        return result.first;
    }
    return m_fileEngine.readLine(data, maxlen);
}

/*!
    \reimp
*/
qint64 RemoteFileEngine::write(const char *data, qint64 len)
{
    if (connectToServer()) {
        QByteArray ba(data, len);
        return callRemoteMethod<qint64>(QString::fromLatin1(Protocol::QAbstractFileEngineWrite), ba);
    }
    return m_fileEngine.write(data, len);
}

bool RemoteFileEngine::syncToDisk()
{
    if (connectToServer())
        return callRemoteMethod<bool>(QString::fromLatin1(Protocol::QAbstractFileEngineSyncToDisk));
    return m_fileEngine.syncToDisk();
}

bool RemoteFileEngine::renameOverwrite(const QString &newName)
{
    if (connectToServer()) {
        return callRemoteMethod<bool>
            (QString::fromLatin1(Protocol::QAbstractFileEngineRenameOverwrite), newName);
    }
    return m_fileEngine.renameOverwrite(newName);
}

QDateTime RemoteFileEngine::fileTime(FileTime time) const
{
    if ((const_cast<RemoteFileEngine *>(this))->connectToServer()) {
        return callRemoteMethod<QDateTime>
            (QString::fromLatin1(Protocol::QAbstractFileEngineFileTime),
            static_cast<qint32> (time));
    }
    return m_fileEngine.fileTime(time);
}

} // namespace QInstaller
