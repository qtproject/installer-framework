/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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
#include <zipjob.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QMetaType>
#include <QtCore/QStringList>

#include <cassert>
#include <climits>

class ZipJob::Private
{
public:
    Private() : outputDevice(0), process(0) {}

    QIODevice *outputDevice;
    QDir workingDir;
    QProcess *process;
    QStringList filesToArchive;
};

Q_DECLARE_METATYPE(QProcess::ExitStatus)

ZipJob::ZipJob() 
    : d(new Private())
{
    qRegisterMetaType<QProcess::ExitStatus>();
}

ZipJob::~ZipJob()
{
    delete d;
}

void ZipJob::run()
{
    assert(!d->process);
    d->process = new QProcess;
    d->process->setWorkingDirectory(d->workingDir.absolutePath());
    QStringList args;
    args << QLatin1String( "-" ) << QLatin1String( "-r" ) << d->filesToArchive;
    connect(d->process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(d->process, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(processFinished(int,QProcess::ExitStatus)));
    connect(d->process, SIGNAL(readyReadStandardOutput()), this, SLOT(processReadyReadStandardOutput()));

    d->process->start(QLatin1String("zip"), args);
    if (!d->process->waitForStarted()) {
        //TODO handle
    }

    if (!d->process->waitForFinished(INT_MAX)) {
        //TODO handle
    }

    delete d->process;
    d->process = 0;
    // emit result
}
    
void ZipJob::processError(QProcess::ProcessError)
{
    emit error();
}

void ZipJob::processFinished(int, QProcess::ExitStatus)
{
    emit finished();
}

void ZipJob::processReadyReadStandardOutput()
{
    const QByteArray buf = d->process->readAll();
    const qint64 toWrite = buf.size();
    qint64 written = 0;
    while (written < toWrite) {
        const qint64 num = d->outputDevice->write(buf.constData() + written, toWrite - written);
        if (num < 0) {
            //TODO: handle error
            return;
        }
        written += num;
    }
}

void ZipJob::setOutputDevice(QIODevice *device)
{
    d->outputDevice = device;
}

void ZipJob::setWorkingDirectory(const QDir &dir)
{
    d->workingDir = dir;
}

void ZipJob::setFilesToArchive(const QStringList &files)
{
    d->filesToArchive = files;
}

class UnzipJob::Private
{
public:
    Private() : inputDevice(0) {}

    QIODevice *inputDevice;
    QString outputPath;
    QStringList filesToExtract;
};

UnzipJob::UnzipJob() 
    : d(new Private())
{
    qRegisterMetaType<QProcess::ExitStatus>();
}

UnzipJob::~UnzipJob()
{
    delete d;
}

void UnzipJob::setInputDevice(QIODevice *device)
{
    d->inputDevice = device;
}

void UnzipJob::setOutputPath(const QString &path)
{
    d->outputPath = path;
}

void UnzipJob::processError(QProcess::ProcessError)
{
    emit error();
}

void UnzipJob::run()
{
    QProcess process;
    // TODO: this won't work on Windows... grmpfl, but on Mac and Linux, at least... 
    QStringList args; 
    args << QLatin1String( "/dev/stdin" );
    if (!d->filesToExtract.isEmpty())
        args << QLatin1String("-x") << d->filesToExtract;
    process.setWorkingDirectory(d->outputPath);
    process.start(QLatin1String("unzip"), args);
    connect(&process, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(&process, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus )));
    if (!process.waitForStarted()) {
        // TODO handle
        return;
    }

    const int bufferSize = 4096;
    QByteArray buffer;
    while (d->inputDevice->bytesAvailable() > 0 || d->inputDevice->waitForReadyRead(INT_MAX)) {
        buffer = d->inputDevice->read(bufferSize);
        process.write(buffer);
        process.waitForBytesWritten(INT_MAX);
    }
    process.closeWriteChannel();

    if (!process.waitForFinished(INT_MAX)) {
        // TODO handle
    }
}

void UnzipJob::processFinished(int, QProcess::ExitStatus)
{
    emit finished();
}

void UnzipJob::setFilesToExtract(const QStringList &files)
{
    d->filesToExtract = files;
}
