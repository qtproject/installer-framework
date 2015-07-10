/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
class QFileInfo;
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
    bool INSTALLER_EXPORT isInBundle(const QString &path, QString *bundlePath = 0);

    QString replacePath(const QString &path, const QString &pathBefore, const QString &pathAfter);

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
