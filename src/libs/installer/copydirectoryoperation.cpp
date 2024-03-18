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

#include "copydirectoryoperation.h"

#include <QtCore/QDir>
#include <QtCore/QDirIterator>
#include <QtCore/QFileInfo>

using namespace QInstaller;

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::CopyDirectoryOperation
    \internal
*/

class AutoPush
{
public:
    AutoPush(CopyDirectoryOperation *op)
        : m_op(op) {}
    ~AutoPush() { m_op->setValue(QLatin1String("files"), m_files); }

    QStringList m_files;
    CopyDirectoryOperation *m_op;
};


CopyDirectoryOperation::CopyDirectoryOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("CopyDirectory"));
}

void CopyDirectoryOperation::backup()
{
}

bool CopyDirectoryOperation::performOperation()
{
    if (!checkArgumentCount(2, 3, tr("<source> <target> [\"forceOverwrite\"]")))
        return false;

    const QStringList args = parsePerformOperationArguments();
    const QString sourcePath = args.at(0);
    const QString targetPath = args.at(1);
    bool overwrite = false;

    if (args.count() > 2) {
        const QString overwriteStr = args.at(2);
        if (overwriteStr == QLatin1String("forceOverwrite")) {
            overwrite = true;
        } else {
            setError(InvalidArguments);
            setErrorString(tr("Invalid argument in %1: Third argument needs to be forceOverwrite, "
                              "if specified.").arg(name()));
            return false;
        }
    }

    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo targetInfo(targetPath);

    foreach (const QFileInfo &dir, QList<QFileInfo>() << sourceInfo << targetInfo) {
        if (!dir.exists() || !dir.isDir()) {
            setError(InvalidArguments);
            setErrorString(tr("Invalid argument in %1: Directory \"%2\" is invalid.").arg(name())
                           .arg(QDir::toNativeSeparators(dir.absolutePath())));
            return false;
        }
    }

    const QDir sourceDir = sourceInfo.absoluteDir();
    const QDir targetDir = targetInfo.absoluteDir();

    AutoPush autoPush(this);
    QDirIterator it(sourceInfo.absoluteFilePath(), QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden,
        QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString itemName = it.next();
        const QFileInfo itemInfo(sourceDir.absoluteFilePath(itemName));
        const QString relativePath = sourceDir.relativeFilePath(itemName);
        if (itemInfo.isSymLink()) {
            // Check if symlink target is inside copied directory
            const QString linkTarget = itemInfo.symLinkTarget();
            if (linkTarget.startsWith(sourceDir.absolutePath())) {
                // create symlink to copied location
                const QString linkTargetRelative = sourceDir.relativeFilePath(linkTarget);
                QFile(targetDir.absoluteFilePath(linkTargetRelative))
                    .link(targetDir.absoluteFilePath(relativePath));
            } else {
                // create symlink pointing to original location
                QFile(linkTarget).link(targetDir.absoluteFilePath(relativePath));
            }
            // add file entry
            autoPush.m_files.prepend(targetDir.absoluteFilePath(relativePath));
            emit outputTextChanged(autoPush.m_files.first());
        } else if (itemInfo.isDir()) {
            if (!targetDir.mkpath(targetDir.absoluteFilePath(relativePath))) {
                setError(InvalidArguments);
                setErrorString(tr("Cannot create directory \"%1\".").arg(
                                   QDir::toNativeSeparators(targetDir.absoluteFilePath(relativePath))));
                return false;
            }
        } else {
            const QString absolutePath = targetDir.absoluteFilePath(relativePath);
            if (overwrite && QFile::exists(absolutePath) && !deleteFileNowOrLater(absolutePath)) {
                setError(UserDefinedError);
                setErrorString(tr("Failed to overwrite \"%1\".").arg(QDir::toNativeSeparators(absolutePath)));
                return false;
            }
            QFile file(sourceDir.absoluteFilePath(itemName));
            if (!file.copy(absolutePath)) {
                setError(UserDefinedError);
                setErrorString(tr("Cannot copy file \"%1\" to \"%2\": %3").arg(
                                   QDir::toNativeSeparators(sourceDir.absoluteFilePath(itemName)),
                                   QDir::toNativeSeparators(targetDir.absoluteFilePath(relativePath)),
                                   file.errorString()));
                return false;
            }
            autoPush.m_files.prepend(targetDir.absoluteFilePath(relativePath));
            emit outputTextChanged(autoPush.m_files.first());
        }
    }
    return true;
}

bool CopyDirectoryOperation::undoOperation()
{
    if (skipUndoOperation())
        return true;

    if (!checkArgumentCount(2))
        return false;

    QDir dir;
    const QStringList files = value(QLatin1String("files")).toStringList();
    foreach (const QString &file, files) {
        if (!QFile::remove(file)) {
            setError(InvalidArguments);
            setErrorString(tr("Cannot remove file \"%1\".").arg(QDir::toNativeSeparators(file)));
            return false;
        }
        dir.rmdir(QFileInfo(file).absolutePath());
        emit outputTextChanged(file);
    }

    setValue(QLatin1String("files"), QStringList());
    return true;
}

bool CopyDirectoryOperation::testOperation()
{
    return true;
}
