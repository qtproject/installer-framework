/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "qtpatch.h"
#include "utils.h"
#include "globals.h"

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QVector>
#include <QTime>
#include <QtCore/QDebug>
#include <QCoreApplication>
#include <QByteArrayMatcher>

QHash<QString, QByteArray> QtPatch::readQmakeOutput(const QByteArray &data)
{
    QHash<QString, QByteArray> qmakeValueHash;
    QTextStream stream(data, QIODevice::ReadOnly);
    while (!stream.atEnd()) {
        const QString line = stream.readLine();
        const int index = line.indexOf(QLatin1Char(':'));
        if (index != -1) {
            QString value = line.mid(index+1);
            if (value != QLatin1String("**Unknown**") )
                qmakeValueHash.insert(line.left(index), value.toUtf8());
        }
    }
    return qmakeValueHash;
}

QHash<QString, QByteArray> QtPatch::qmakeValues(const QString &qmakePath, QByteArray *qmakeOutput)
{
    QHash<QString, QByteArray> qmakeValueHash;

    // in some cases qmake is not runable, because another process is blocking it(filewatcher ...)
    int waitCount = 0;
    while (qmakeValueHash.isEmpty() && waitCount < 3) {
        QFileInfo qmake(qmakePath);

        if (!qmake.exists()) {
            qmakeOutput->append(QString::fromLatin1("%1 is not existing").arg(qmakePath).toUtf8());
            return qmakeValueHash;
        }
        if (!qmake.isExecutable()) {
            qmakeOutput->append(QString::fromLatin1("%1 is not executable").arg(qmakePath).toUtf8());
            return qmakeValueHash;
        }

        QStringList args;
        args << QLatin1String("-query");

        QProcess process;
        process.start(qmake.absoluteFilePath(), args, QIODevice::ReadOnly);
        if (process.waitForFinished(10000)) {
            QByteArray output = process.readAllStandardOutput();
            if ((process.exitStatus() == QProcess::CrashExit) || (process.exitCode() != EXIT_SUCCESS)) {
                QStringList detailedOutput = { qmake.absoluteFilePath() + QLatin1Char(' ') + args.join(QLatin1Char(' '))
                    + QString::fromLatin1(" returned with exit code: \"%1\".").arg(QString::number(process.exitCode()))
                    , QString::fromLatin1("Standard output: \"%1\".").arg(QLatin1String(output))
                    , QString::fromLatin1("Error output: \"%1\".").arg(QLatin1String(process.readAllStandardError()))
                };
                qmakeOutput->append(detailedOutput.join(QLatin1Char('\n')).toUtf8());
                return qmakeValueHash;
            }
            qmakeOutput->append(output);
            qmakeValueHash = readQmakeOutput(output);
        }
        if (qmakeValueHash.isEmpty()) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            QInstaller::uiDetachedWait(waitTimeInMilliSeconds);
        }
        if (process.state() > QProcess::NotRunning ) {
            qCWarning(QInstaller::lcInstallerInstallLog) << "qmake process is still running, need to kill it.";
            process.kill();
        }

    }
    if (qmakeValueHash.isEmpty())
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot get any query output from qmake.";
    return qmakeValueHash;
}

bool QtPatch::patchBinaryFile(const QString &fileName,
                              const QByteArray &oldQtPath,
                              const QByteArray &newQtPath)
{
    QFile file(fileName);
    if (!file.exists()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "qpatch: warning: file" << fileName << "not found";
        return false;
    }

    openFileForPatching(&file);
    if (!file.isOpen()) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "qpatch: warning: file" << qPrintable(fileName)
            << "cannot open.";
        qCWarning(QInstaller::lcInstallerInstallLog).noquote() << file.errorString();
        return false;
    }

    bool isPatched = patchBinaryFile(&file, oldQtPath, newQtPath);

    file.close();
    return isPatched;
}

// device must be open
bool QtPatch::patchBinaryFile(QIODevice *device,
                              const QByteArray &oldQtPath,
                              const QByteArray &newQtPath)
{
    if (!(device->openMode() == QIODevice::ReadWrite)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "qpatch: warning: This function needs an "
            "open device for writing.";
        return false;
    }
    const QByteArray source = device->readAll();
    device->seek(0);

    int offset = 0;
    QByteArray overwritePath(newQtPath);
    if (overwritePath.size() < oldQtPath.size()) {
        QByteArray fillByteArray(oldQtPath.size() - overwritePath.size(), '\0');
        overwritePath.append(fillByteArray);
    }

    QByteArrayMatcher byteArrayMatcher(oldQtPath);
    forever {
        offset = byteArrayMatcher.indexIn(source, offset);
        if (offset == -1)
            break;
        device->seek(offset);
        device->write(overwritePath);
        offset += overwritePath.size();
    }
    device->seek(0); //for next reading we should be at the beginning
    return true;
}

bool QtPatch::patchTextFile(const QString &fileName,
                            const QHash<QByteArray, QByteArray> &searchReplacePairs)
{
    QFile file(fileName);

    if (!file.open(QFile::ReadOnly)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "Cannot open file" << fileName << "for patching:"
            << file.errorString();
        return false;
    }

    QByteArray source = file.readAll();
    file.close();

    QHashIterator<QByteArray, QByteArray> it(searchReplacePairs);
    while (it.hasNext()) {
        it.next();
        source.replace(it.key(), it.value());
    }

    if (!file.open(QFile::WriteOnly | QFile::Truncate)) {
        qCWarning(QInstaller::lcInstallerInstallLog) << "File" << fileName << "not writable.";
        return false;
    }

    file.write(source);
    return true;
}

bool QtPatch::openFileForPatching(QFile *file)
{
    if (file->openMode() == QIODevice::NotOpen) {
        // in some cases the file cannot be opened, because another process is blocking it (filewatcher ...)
        int waitCount = 0;
        while (!file->open(QFile::ReadWrite) && waitCount < 60) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            QInstaller::uiDetachedWait(waitTimeInMilliSeconds);
        }
        return file->openMode() == QFile::ReadWrite;
    }
    qCWarning(QInstaller::lcInstallerInstallLog) << "File" << file->fileName() << "is open, "
        "so it cannot be opened again.";
    return false;
}
