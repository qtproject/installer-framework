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

#ifndef ZIPJOB_H
#define ZIPJOB_H

#include <QProcess>
#include <QRunnable>

QT_BEGIN_NAMESPACE
class QDir;
class QIODevice;
class QStringList;
QT_END_NAMESPACE

class ZipJob : public QObject, public QRunnable
{
    Q_OBJECT

public:
    ZipJob();
    ~ZipJob();

    void setOutputDevice(QIODevice *device);
    void setWorkingDirectory(const QDir &dir);
    void setFilesToArchive(const QStringList &files);

    void run();

Q_SIGNALS:
    void finished();
    void error();

private Q_SLOTS:
    void processError(QProcess::ProcessError);
    void processFinished(int, QProcess::ExitStatus);
    void processReadyReadStandardOutput();

private:
    class Private;
    Private *const d;
};

class UnzipJob : public QObject, public QRunnable
{
    Q_OBJECT

public:
    UnzipJob();
    ~UnzipJob();

    void setInputDevice(QIODevice *device);
    void setOutputPath(const QString &path);
    void setFilesToExtract(const QStringList &files);

    void run();

Q_SIGNALS:
    void finished();
    void error();

private Q_SLOTS:
    void processError(QProcess::ProcessError);
    void processFinished(int, QProcess::ExitStatus);

private:
    class Private;
    Private *const d;
};

#endif // ZIPJOB_H
