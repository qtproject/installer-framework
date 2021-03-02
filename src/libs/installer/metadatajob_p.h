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

#ifndef METADATAJOB_P_H
#define METADATAJOB_P_H

#include "lib7zarchive.h"
#include "metadatajob.h"

#include <QDir>
#include <QFile>

namespace QInstaller{

class UnzipArchiveException : public QException
{
public:
    UnzipArchiveException() {}
    ~UnzipArchiveException() throw() {}
    explicit UnzipArchiveException(const QString &message)
        : m_message(message)
    {}

    void raise() const { throw *this; }
    QString message() const { return m_message; }
    UnzipArchiveException *clone() const { return new UnzipArchiveException(*this); }

private:
    QString m_message;
};

class UnzipArchiveTask : public AbstractTask<void>
{
    Q_OBJECT
    Q_DISABLE_COPY(UnzipArchiveTask)

public:
    UnzipArchiveTask(const QString &arcive, const QString &target)
        : m_archive(arcive), m_targetDir(target)
    {}
    QString target() { return m_targetDir; }
    QString archive() { return m_archive; }
    void doTask(QFutureInterface<void> &fi)
    {
        fi.reportStarted();
        fi.setExpectedResultCount(1);

        if (fi.isCanceled()) {
            fi.reportFinished();
            return; // ignore already canceled
        }

        Lib7zArchive archive(m_archive);
        if (!archive.open(QIODevice::ReadOnly)) {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Cannot open file \"%1\" for "
                "reading: %2").arg(QDir::toNativeSeparators(m_archive), archive.errorString())));
        }
        if (!archive.extract(m_targetDir)) {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Error while extracting "
                "archive \"%1\": %2").arg(QDir::toNativeSeparators(m_archive), archive.errorString())));
        }

        fi.reportFinished();
    }

private:
    QString m_archive;
    QString m_targetDir;
};

}   // namespace QInstaller

#endif  // METADATAJOB_P_H
