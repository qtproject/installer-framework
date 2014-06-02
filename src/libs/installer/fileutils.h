/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
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
#ifndef QINSTALLER_FILEUTILS_H
#define QINSTALLER_FILEUTILS_H

#include "installer_global.h"

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>

QT_BEGIN_NAMESPACE
class QByteArray;
class QFileInfo;
class QIODevice;
class QUrl;
QT_END_NAMESPACE

namespace QInstaller {
class INSTALLER_EXPORT TempDirDeleter
{
public:
    explicit TempDirDeleter(const QString &path);
    explicit TempDirDeleter(const QStringList &paths = QStringList());
    ~TempDirDeleter();

    QStringList paths() const;

    void add(const QString &path);
    void add(const QStringList &paths);

    void releaseAll();
    void release(const QString &path);
    void passAndReleaseAll(TempDirDeleter &tdd);
    void passAndRelease(TempDirDeleter &tdd, const QString &path);

    void releaseAndDeleteAll();
    void releaseAndDelete(const QString &path);

private:
    Q_DISABLE_COPY(TempDirDeleter)
    QSet<QString> m_paths;
};

    QString INSTALLER_EXPORT humanReadableSize(const qint64 &size, int precision = 2);

    void INSTALLER_EXPORT openForRead(QIODevice *dev, const QString &name);
    void INSTALLER_EXPORT openForWrite(QIODevice *dev, const QString &name);
    void INSTALLER_EXPORT openForAppend(QIODevice *dev, const QString &name);

    qint64 INSTALLER_EXPORT blockingRead(QIODevice *in, char *buffer, qint64 size);
    void INSTALLER_EXPORT blockingCopy(QIODevice *in, QIODevice *out, qint64 size);
    qint64 INSTALLER_EXPORT blockingWrite(QIODevice *out, const char *buffer, qint64 size);
    qint64 INSTALLER_EXPORT blockingWrite(QIODevice *out, const QByteArray& ba);

    /*!
        Removes the directory at \a path recursively.
        @param path The directory to remove
        @param ignoreErrors if @p true, errors will be silently ignored. Otherwise an exception will be thrown
            if removing fails.

        @throws QInstaller::Error if the directory cannot be removed and ignoreErrors is @p false
    */
    void INSTALLER_EXPORT removeFiles(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeDirectory(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeDirectoryThreaded(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeSystemGeneratedFiles(const QString &path);

    QString INSTALLER_EXPORT generateTemporaryFileName(const QString &templ=QString());

    void INSTALLER_EXPORT moveDirectoryContents(const QString &sourceDir, const QString &targetDir);
    void INSTALLER_EXPORT copyDirectoryContents(const QString &sourceDir, const QString &targetDir);

    bool INSTALLER_EXPORT isLocalUrl(const QUrl &url);
    QString INSTALLER_EXPORT pathFromUrl(const QUrl &url);

    void INSTALLER_EXPORT mkdir(const QString &path);
    void INSTALLER_EXPORT mkpath(const QString &path);

    quint64 INSTALLER_EXPORT fileSize(const QFileInfo &info);

#ifdef Q_OS_WIN
    QString INSTALLER_EXPORT getLongPathName(const QString &name);
    QString INSTALLER_EXPORT getShortPathName(const QString &name);

    /*!
        Makes sure that capitalization of directories is canonical.
    */
    QString INSTALLER_EXPORT normalizePathName(const QString &name);

    /*!
        Sets the .ico file at \a icon as application icon for \a application.
    */
    void INSTALLER_EXPORT setApplicationIcon(const QString &application, const QString &icon);
#endif
}

#endif // QINSTALLER_FILEUTILS_H
