/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "qtpatch.h"

#include "common/utils.h"

#include <QString>
#include <QStringList>
#include <QFileInfo>
#include <QProcess>
#include <QTextStream>
#include <QVector>
#include <QTime>
#include <QDebug>
#include <QCoreApplication>
#include <QByteArrayMatcher>

#ifdef Q_OS_WIN
#include <windows.h> // for Sleep
#endif
#ifdef Q_OS_UNIX
#include <errno.h>
#include <signal.h>
#include <time.h>
#endif

static void sleepCopiedFromQTest(int ms)
{
    if (ms < 0)
        return;
#ifdef Q_OS_WIN
    Sleep(uint(ms));
#else
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000 * 1000 };
    nanosleep(&ts, NULL);
#endif
}

static void uiDetachedWait(int ms)
{
    QTime timer;
    timer.start();
    do {
        QCoreApplication::processEvents(QEventLoop::AllEvents, ms);
        sleepCopiedFromQTest(10);
    } while (timer.elapsed() < ms);
}

QHash<QString, QByteArray> QtPatch::qmakeValues(const QString &qmakePath, QByteArray *qmakeOutput)
{
    QHash<QString, QByteArray> qmakeValueHash;

    // in some cases qmake is not runable, because another process is blocking it(filewatcher ...)
    int waitCount = 0;
    while (qmakeValueHash.isEmpty() && waitCount < 60) {
        QFileInfo qmake(qmakePath);

        if (!qmake.exists()) {
            qmakeOutput->append(QString::fromLatin1("%1 is not existing").arg(qmakePath));
            return qmakeValueHash;
        }
        if (!qmake.isExecutable()) {
            qmakeOutput->append(QString::fromLatin1("%1 is not executable").arg(qmakePath));
            return qmakeValueHash;
        }

        QStringList args;
        args << QLatin1String("-query");

        QProcess process;
        process.start(qmake.absoluteFilePath(), args, QIODevice::ReadOnly);
        if (process.waitForFinished(2000)) {
            if (process.exitStatus() == QProcess::CrashExit) {
                QInstaller::verbose() << qmakePath << " was crashed" << std::endl;
                return qmakeValueHash;
            }
            QByteArray output = process.readAllStandardOutput();
            qmakeOutput->append(output);
            QTextStream stream(&output);
            while (!stream.atEnd()) {
                const QString line = stream.readLine();
                const int index = line.indexOf(QLatin1Char(':'));
                if (index != -1) {
                    QString value = line.mid(index+1);
                    if (value != QLatin1String("**Unknown**") )
                        qmakeValueHash.insert(line.left(index), value.toUtf8());
                }
            }
        }
        if (qmakeValueHash.isEmpty()) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            uiDetachedWait(waitTimeInMilliSeconds);
        }
        if (process.state() > QProcess::NotRunning ) {
            QInstaller::verbose() << "qmake process is still running, need to kill it." << std::endl;
            process.kill();
        }

    }
    if (qmakeValueHash.isEmpty())
        QInstaller::verbose() << "Can't get any query output from qmake." << std::endl;
    return qmakeValueHash;
}

bool QtPatch::patchBinaryFile(const QString &fileName,
                              const QByteArray &oldQtPath,
                              const QByteArray &newQtPath)
{
    QFile file(fileName);
    if (!file.exists()) {
        QInstaller::verbose() << "qpatch: warning: file `" << qPrintable(fileName) << "' not found" << std::endl;
        return false;
    }

    openFileForPatching(&file);
    if (!file.isOpen()) {
        QInstaller::verbose() << "qpatch: warning: file `" << qPrintable(fileName) << "' can not open." << std::endl;
        QInstaller::verbose() << qPrintable(file.errorString()) << std::endl;
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
        QInstaller::verbose() << "qpatch: warning: This function needs an open device for writing." << std::endl;
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
        QInstaller::verbose() << "qpatch: warning: Open the file '"
                              << qPrintable(fileName) << "' stopped: "
                              << qPrintable(file.errorString()) << std::endl;
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
        QInstaller::verbose() << "qpatch: error: file `" << qPrintable(fileName) << "' not writable" << std::endl;
        return false;
    }

    file.write(source);
    return true;
}

bool QtPatch::openFileForPatching(QFile *file)
{
    if (file->openMode() == QIODevice::NotOpen) {
        // in some cases the file can not open, because another process is blocking it(filewatcher ...)
        int waitCount = 0;
        while (!file->open(QFile::ReadWrite) && waitCount < 60) {
            ++waitCount;
            static const int waitTimeInMilliSeconds = 500;
            uiDetachedWait(waitTimeInMilliSeconds);
        }
        return file->openMode() == QFile::ReadWrite;
    }
    QInstaller::verbose() << "qpatch: error: File `" << qPrintable(file->fileName()) << "' is open, so it can not open it again." << std::endl;
    return false;
}
