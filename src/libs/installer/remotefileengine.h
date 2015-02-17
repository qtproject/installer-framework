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
    QAbstractFileEngine* create(const QString &fileName) const Q_DECL_OVERRIDE;
};

class RemoteFileEngine : public RemoteObject, public QAbstractFileEngine
{
    Q_DISABLE_COPY(RemoteFileEngine)

public:
    RemoteFileEngine();
    ~RemoteFileEngine();

    bool open(QIODevice::OpenMode mode) Q_DECL_OVERRIDE;
    bool close() Q_DECL_OVERRIDE;
    bool flush() Q_DECL_OVERRIDE;
    bool syncToDisk() Q_DECL_OVERRIDE;
    qint64 size() const Q_DECL_OVERRIDE;
    qint64 pos() const Q_DECL_OVERRIDE;
    bool seek(qint64 offset) Q_DECL_OVERRIDE;
    bool isSequential() const Q_DECL_OVERRIDE;
    bool remove() Q_DECL_OVERRIDE;
    bool copy(const QString &newName) Q_DECL_OVERRIDE;
    bool rename(const QString &newName) Q_DECL_OVERRIDE;
    bool renameOverwrite(const QString &newName) Q_DECL_OVERRIDE;
    bool link(const QString &newName) Q_DECL_OVERRIDE;
    bool mkdir(const QString &dirName, bool createParentDirectories) const Q_DECL_OVERRIDE;
    bool rmdir(const QString &dirName, bool recurseParentDirectories) const Q_DECL_OVERRIDE;
    bool setSize(qint64 size) Q_DECL_OVERRIDE;
    bool caseSensitive() const Q_DECL_OVERRIDE;
    bool isRelativePath() const Q_DECL_OVERRIDE;
    QStringList entryList(QDir::Filters filters, const QStringList &filterNames) const Q_DECL_OVERRIDE;
    FileFlags fileFlags(FileFlags type = FileInfoAll) const Q_DECL_OVERRIDE;
    bool setPermissions(uint perms) Q_DECL_OVERRIDE;
    QString fileName(FileName file = DefaultName) const Q_DECL_OVERRIDE;
    uint ownerId(FileOwner owner) const Q_DECL_OVERRIDE;
    QString owner(FileOwner owner) const Q_DECL_OVERRIDE;
    QDateTime fileTime(FileTime time) const Q_DECL_OVERRIDE;
    void setFileName(const QString &fileName) Q_DECL_OVERRIDE;
    int handle() const Q_DECL_OVERRIDE;
    bool atEnd() const;
    uchar *map(qint64, qint64, QFile::MemoryMapFlags) { return 0; }
    bool unmap(uchar *) { return true; }

    Iterator *beginEntryList(QDir::Filters filters, const QStringList &filterNames) Q_DECL_OVERRIDE;
    Iterator *endEntryList() Q_DECL_OVERRIDE;

    qint64 read(char *data, qint64 maxlen) Q_DECL_OVERRIDE;
    qint64 readLine(char *data, qint64 maxlen) Q_DECL_OVERRIDE;
    qint64 write(const char *data, qint64 len) Q_DECL_OVERRIDE;

    QFile::FileError error() const;
    QString errorString() const;

    bool extension(Extension extension, const ExtensionOption *option = 0,
        ExtensionReturn *output = 0) Q_DECL_OVERRIDE;
    bool supportsExtension(Extension extension) const Q_DECL_OVERRIDE;

private:
    QFSFileEngine m_fileEngine;
};

} // namespace QInstaller

#endif // REMOTEFILEENGINE_H
