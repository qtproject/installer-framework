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

#ifndef EXTRACTARCHIVEOPERATION_H
#define EXTRACTARCHIVEOPERATION_H

#include "qinstallerglobal.h"

#include <QtCore/QObject>

namespace QInstaller {

class INSTALLER_EXPORT ExtractArchiveOperation : public QObject, public Operation
{
    Q_OBJECT
    friend class WorkerThread;

public:
    explicit ExtractArchiveOperation(PackageManagerCore *core);

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;

    quint64 sizeHint() override;

    bool readDataFileContents(QString &targetDir, QStringList *resultList);

Q_SIGNALS:
    void outputTextChanged(const QString &progress);
    void progressChanged(double);

private:
    void startUndoProcess(const QStringList &files);
    void deleteDataFile(const QString &fileName);

    QString generateBackupName(const QString &fn);
    bool prepareForFile(const QString &filename);

private:
    typedef QPair<QString, QString> Backup;
    typedef QVector<Backup> BackupFiles;

    class Callback;
    class Worker;
    class Receiver;

private:
    QString m_relocatedDataFileName;
    BackupFiles m_backupFiles;
    quint64 m_totalEntries;
};

}

#endif
