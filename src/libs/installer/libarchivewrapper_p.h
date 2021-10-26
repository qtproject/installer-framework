/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#ifndef LIBARCHIVEWRAPPER_P_H
#define LIBARCHIVEWRAPPER_P_H

#include "installer_global.h"
#include "remoteobject.h"
#include "libarchivearchive.h"

#include <QTimer>
#include <QReadWriteLock>

namespace QInstaller {

class INSTALLER_EXPORT LibArchiveWrapperPrivate : public RemoteObject
{
    Q_OBJECT
    Q_DISABLE_COPY(LibArchiveWrapperPrivate)

public:
    explicit LibArchiveWrapperPrivate(const QString &filename);
    LibArchiveWrapperPrivate();
    ~LibArchiveWrapperPrivate();

    bool open(QIODevice::OpenMode mode);
    void close();
    void setFilename(const QString &filename);

    QString errorString() const;

    bool extract(const QString &dirPath, const quint64 totalFiles = 0);
    bool create(const QStringList &data);
    QVector<ArchiveEntry> list();
    bool isSupported();

    void setCompressionLevel(const AbstractArchive::CompressionLevel level);

Q_SIGNALS:
    void currentEntryChanged(const QString &filename);
    void completedChanged(const quint64 completed, const quint64 total);
    void dataBlockRequested();
    void seekRequested(qint64 offset, int whence);
    void remoteWorkerFinished();

public Q_SLOTS:
    void cancel();

private Q_SLOTS:
    void processSignals();
    void onDataBlockRequested();
    void onSeekRequested(qint64 offset, int whence);

private:
    void init();

    void addDataBlock(const QByteArray &buffer);
    void setClientDataAtEnd();
    void setClientFilePosition(qint64 pos);
    ExtractWorker::Status workerStatus() const;

private:
    mutable QReadWriteLock m_lock;

    LibArchiveArchive m_archive;
};

} // namespace QInstaller

#endif // LIBARCHIVEWRAPPER_P_H
