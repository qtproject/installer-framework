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

#ifndef LIB7ZARCHIVE_H
#define LIB7ZARCHIVE_H

#include "installer_global.h"
#include "abstractarchive.h"
#include "lib7z_extract.h"

namespace QInstaller {

class INSTALLER_EXPORT Lib7zArchive : public AbstractArchive
{
    Q_OBJECT
    Q_DISABLE_COPY(Lib7zArchive)

public:
    Lib7zArchive(const QString &filename, QObject *parent = nullptr);
    explicit Lib7zArchive(QObject *parent = nullptr);
    ~Lib7zArchive();

    bool open(QIODevice::OpenMode mode) override;
    void close() override;
    void setFilename(const QString &filename) override;

    bool extract(const QString &dirPath) override;
    bool extract(const QString &dirPath, const quint64 totalFiles) override;
    bool create(const QStringList &data) override;
    QVector<ArchiveEntry> list() override;
    bool isSupported() override;

public Q_SLOTS:
    void cancel() override;

private:
    void listenExtractCallback();

    class ExtractCallbackWrapper;

private:
    QFile m_file;
    ExtractCallbackWrapper *const m_extractCallback;
};

class Lib7zArchive::ExtractCallbackWrapper : public QObject, public Lib7z::ExtractCallback
{
    Q_OBJECT
    Q_DISABLE_COPY(ExtractCallbackWrapper)

public:
    ExtractCallbackWrapper();

    void setState(HRESULT state);

Q_SIGNALS:
    void currentEntryChanged(const QString &filename);
    void completedChanged(quint64 completed, quint64 total);

private:
    void setCurrentFile(const QString &filename) override;
    HRESULT setCompleted(quint64 completed, quint64 total) override;

private:
    HRESULT m_state;
};

} // namespace QInstaller

#endif // LIB7ZARCHIVE_H
