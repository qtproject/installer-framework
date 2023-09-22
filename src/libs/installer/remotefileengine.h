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

#ifndef REMOTEFILEENGINE_H
#define REMOTEFILEENGINE_H

#include "remoteobject.h"

#include <QtCore/private/qabstractfileengine_p.h>
#include <QtCore/private/qfsfileengine_p.h>

namespace QInstaller {

class INSTALLER_EXPORT RemoteFileEngineHandler : public QAbstractFileEngineHandler
{
    Q_DISABLE_COPY(RemoteFileEngineHandler)

public:
    RemoteFileEngineHandler() : QAbstractFileEngineHandler() {}
    QAbstractFileEngine* create(const QString &fileName) const override;
};

class RemoteFileEngine : public RemoteObject, public QAbstractFileEngine
{
    Q_DISABLE_COPY(RemoteFileEngine)

public:
    RemoteFileEngine();
    ~RemoteFileEngine();

#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    bool open(QIODevice::OpenMode mode) override;
#else
    bool open(QIODevice::OpenMode mode,
              std::optional<QFile::Permissions> permissions = std::nullopt) override;
#endif
    bool close() override;
    bool flush() override;
    bool syncToDisk() override;
    qint64 size() const override;
    qint64 pos() const override;
    bool seek(qint64 offset) override;
    bool isSequential() const override;
    bool remove() override;
    bool copy(const QString &newName) override;
    bool rename(const QString &newName) override;
    bool renameOverwrite(const QString &newName) override;
    bool link(const QString &newName) override;
#if QT_VERSION < QT_VERSION_CHECK(6, 3, 0)
    bool mkdir(const QString &dirName, bool createParentDirectories) const override;
#else
    bool mkdir(const QString &dirName, bool createParentDirectories,
               std::optional<QFile::Permissions> permissions = std::nullopt) const override;
#endif
    bool rmdir(const QString &dirName, bool recurseParentDirectories) const override;
    bool setSize(qint64 size) override;
    bool caseSensitive() const override;
    bool isRelativePath() const override;
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const override;
    FileFlags fileFlags(FileFlags type = FileInfoAll) const override;
    bool setPermissions(uint perms) override;
    QString fileName(FileName file = DefaultName) const override;
    uint ownerId(FileOwner owner) const override;
    QString owner(FileOwner owner) const override;
    QDateTime fileTime(FileTime time) const override;
    void setFileName(const QString &fileName) override;
    int handle() const override;
    bool atEnd() const;
    uchar *map(qint64, qint64, QFile::MemoryMapFlags) { return 0; }
    bool unmap(uchar *) { return true; }

    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames) override;
    Iterator *endEntryList() override;

    qint64 read(char *data, qint64 maxlen) override;
    qint64 readLine(char *data, qint64 maxlen) override;
    qint64 write(const char *data, qint64 len) override;

    QFile::FileError error() const;
    QString errorString() const;

    bool extension(Extension extension, const ExtensionOption *option = 0,
        ExtensionReturn *output = 0) override;
    bool supportsExtension(Extension extension) const override;

private:
    QFSFileEngine m_fileEngine;
};

} // namespace QInstaller

#endif // REMOTEFILEENGINE_H
