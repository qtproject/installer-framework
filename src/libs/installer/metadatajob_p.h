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

#ifndef METADATAJOB_P_H
#define METADATAJOB_P_H

#include "lib7z_facade.h"
#include "metadatajob.h"

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

    void doTask(QFutureInterface<void> &fi)
    {
        fi.reportStarted();
        fi.setExpectedResultCount(1);

        if (fi.isCanceled()) {
            fi.reportFinished();
            return; // ignore already canceled
        }

        QFile archive(m_archive);
        if (archive.open(QIODevice::ReadOnly)) {
            try {
                Lib7z::extractArchive(&archive, m_targetDir);
            } catch (const Lib7z::SevenZipException& e) {
                fi.reportException(UnzipArchiveException(MetadataJob::tr("Error while extracting "
                    "'%1': %2").arg(m_archive, e.message())));
            } catch (...) {
                fi.reportException(UnzipArchiveException(MetadataJob::tr("Unknown exception "
                    "caught while extracting %1.").arg(m_archive)));
            }
        } else {
            fi.reportException(UnzipArchiveException(MetadataJob::tr("Could not open %1 for "
                "reading. Error: %2").arg(m_archive, archive.errorString())));
        }

        fi.reportFinished();
    }

private:
    QString m_archive;
    QString m_targetDir;
};

}   // namespace QInstaller

#endif  // METADATAJOB_P_H
