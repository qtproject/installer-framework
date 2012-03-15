/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#include "qtpatch.h"

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
                qDebug() << qmakePath << "was crashed";
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
            qDebug() << "qmake process is still running, need to kill it.";
            process.kill();
        }

    }
    if (qmakeValueHash.isEmpty())
        qDebug() << "Can't get any query output from qmake.";
    return qmakeValueHash;
}

bool QtPatch::patchBinaryFile(const QString &fileName,
                              const QByteArray &oldQtPath,
                              const QByteArray &newQtPath)
{
    QFile file(fileName);
    if (!file.exists()) {
        qDebug() << "qpatch: warning: file" << fileName << "not found";
        return false;
    }

    openFileForPatching(&file);
    if (!file.isOpen()) {
        qDebug() << "qpatch: warning: file" << qPrintable(fileName) << "can not open.";
        qDebug() << qPrintable(file.errorString());
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
        qDebug() << "qpatch: warning: This function needs an open device for writing.";
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
        qDebug() << QString::fromLatin1("qpatch: warning: Open the file '%1' stopped: %2").arg(
            fileName, file.errorString());
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
        qDebug() << QString::fromLatin1("qpatch: error: file '%1' not writable").arg(fileName);
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
    qDebug() << QString::fromLatin1("qpatch: error: File '%1 is open, so it can not open it again.").arg(
        file->fileName());
    return false;
}
