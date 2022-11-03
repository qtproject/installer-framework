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
#ifndef QINSTALLER_FILEUTILS_H
#define QINSTALLER_FILEUTILS_H

#include "installer_global.h"

#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtXml/QDomDocument>
#include <QtXml/QDomNodeList>

QT_BEGIN_NAMESPACE
class QFileInfo;
class QFile;
class QUrl;
QT_END_NAMESPACE

namespace QInstaller {

enum DefaultFilePermissions {
    NonExecutable = 0x6644,
    Executable = 0x7755
};

class INSTALLER_EXPORT TempPathDeleter
{
public:
    explicit TempPathDeleter(const QString &path);
    explicit TempPathDeleter(const QStringList &paths = QStringList());
    ~TempPathDeleter();

    QStringList paths() const;

    void add(const QString &path);
    void add(const QStringList &paths);

    void releaseAndDeleteAll();
    void releaseAndDelete(const QString &path);

private:
    Q_DISABLE_COPY(TempPathDeleter)
    QSet<QString> m_paths;
};

    QString INSTALLER_EXPORT humanReadableSize(const qint64 &size, int precision = 2);

    void INSTALLER_EXPORT removeFiles(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeDirectory(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeDirectoryThreaded(const QString &path, bool ignoreErrors = false);
    void INSTALLER_EXPORT removeSystemGeneratedFiles(const QString &path);

    bool INSTALLER_EXPORT setDefaultFilePermissions(const QString &fileName, DefaultFilePermissions permissions);
    bool INSTALLER_EXPORT setDefaultFilePermissions(QFile *file, DefaultFilePermissions permissions);

    QString INSTALLER_EXPORT generateTemporaryFileName(const QString &templ=QString());

    void INSTALLER_EXPORT moveDirectoryContents(const QString &sourceDir, const QString &targetDir);
    void INSTALLER_EXPORT copyDirectoryContents(const QString &sourceDir, const QString &targetDir);

    bool INSTALLER_EXPORT isLocalUrl(const QUrl &url);
    QString INSTALLER_EXPORT pathFromUrl(const QUrl &url);

    void INSTALLER_EXPORT mkdir(const QString &path);
    void INSTALLER_EXPORT mkpath(const QString &path);
    bool INSTALLER_EXPORT createDirectoryWithParents(const QString &path);
#ifdef Q_OS_MACOS
    void INSTALLER_EXPORT mkalias(const QString &path, const QString &alias);
#endif

    quint64 INSTALLER_EXPORT fileSize(const QFileInfo &info);
    bool INSTALLER_EXPORT isInBundle(const QString &path, QString *bundlePath = 0);

    QString replacePath(const QString &path, const QString &pathBefore, const QString &pathAfter, bool cleanPath = true);
    void replaceHighDpiImage(QString &imagePath);

    void INSTALLER_EXPORT trimmedCopyConfigData(const QString &source, const QString &target, const QStringList &elementsToRemoveTags);
    void copyConfigChildElements(QDomDocument &dom, const QDomNodeList &objects, const QString &sourceDir, const QString &targetDir);

#ifdef Q_OS_WIN
    QString INSTALLER_EXPORT getLongPathName(const QString &name);
    QString INSTALLER_EXPORT getShortPathName(const QString &name);

    QString INSTALLER_EXPORT normalizePathName(const QString &name);

    void INSTALLER_EXPORT setApplicationIcon(const QString &application, const QString &icon);
#endif
}

#endif // QINSTALLER_FILEUTILS_H
