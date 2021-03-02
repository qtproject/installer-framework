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

#ifndef LIBARCHIVEWRAPPER_H
#define LIBARCHIVEWRAPPER_H

#include "installer_global.h"
#include "abstractarchive.h"
#include "libarchivewrapper_p.h"

namespace QInstaller {

class INSTALLER_EXPORT LibArchiveWrapper : public AbstractArchive
{
    Q_OBJECT
    Q_DISABLE_COPY(LibArchiveWrapper)

public:
    LibArchiveWrapper(const QString &filename, QObject *parent = nullptr);
    explicit LibArchiveWrapper(QObject *parent = nullptr);
    ~LibArchiveWrapper();

    bool open(QIODevice::OpenMode mode) Q_DECL_OVERRIDE;
    void close() Q_DECL_OVERRIDE;
    void setFilename(const QString &filename) Q_DECL_OVERRIDE;

    QString errorString() const Q_DECL_OVERRIDE;

    bool extract(const QString &dirPath) Q_DECL_OVERRIDE;
    bool extract(const QString &dirPath, const quint64 totalFiles) Q_DECL_OVERRIDE;
    bool create(const QStringList &data) Q_DECL_OVERRIDE;
    QVector<ArchiveEntry> list() Q_DECL_OVERRIDE;
    bool isSupported() Q_DECL_OVERRIDE;

    void setCompressionLevel(const AbstractArchive::CompressionLevel level) Q_DECL_OVERRIDE;

public Q_SLOTS:
    void cancel() Q_DECL_OVERRIDE;

private:
    LibArchiveWrapperPrivate *const d;
};

} // namespace QInstaller

#endif // LIBARCHIVEWRAPPER_H
