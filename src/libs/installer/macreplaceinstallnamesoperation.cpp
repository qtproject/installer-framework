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

#include "macreplaceinstallnamesoperation.h"

#include "qprocesswrapper.h"

#include <QBuffer>
#include <QDebug>
#include <QDirIterator>

using namespace QInstaller;

MacReplaceInstallNamesOperation::MacReplaceInstallNamesOperation()
{
    setName(QLatin1String("ReplaceInstallNames"));
}

void MacReplaceInstallNamesOperation::backup()
{
}

bool MacReplaceInstallNamesOperation::performOperation()
{
    // Arguments:
    // 1. search string, means the beginning till that string will be replaced
    // 2. new/current target install directory(the replacement)
    // 3. directory containing frameworks
    // 4. other directory containing frameworks
    // 5. other directory containing frameworks
    // 6. ...

    if (arguments().count() < 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("at least 3"), QLatin1String("")));
        return false;
    }

    const QString searchString = arguments().at(0);
    const QString installationDir = arguments().at(1);
    const QStringList searchDirList = arguments().mid(2);
    if (searchString.isEmpty() || installationDir.isEmpty() || searchDirList.isEmpty()) {
        setError(InvalidArguments);
        setErrorString(tr("One of the given arguments is empty. Argument1=%1; Argument2=%2, Argument3=%3")
            .arg(searchString, installationDir, searchDirList.join(QLatin1String(" "))));
        return false;
    }
    foreach (const QString &searchDir, searchDirList)
        apply(searchString, installationDir, searchDir);

    return error() == NoError;
}

bool MacReplaceInstallNamesOperation::undoOperation()
{
    return true;
}

bool MacReplaceInstallNamesOperation::testOperation()
{
    return true;
}

Operation *MacReplaceInstallNamesOperation::clone() const
{
    return new MacReplaceInstallNamesOperation;
}

QSet<MacBinaryInfo> MacReplaceInstallNamesOperation::collectPatchableBinaries(const QString &searchDir)
{
    QSet<MacBinaryInfo> patchableBinaries;
    QDirIterator dirIterator(searchDir, QDirIterator::Subdirectories);
    while (dirIterator.hasNext()) {
        const QString fileName = dirIterator.next();
        const QFileInfo fileInfo = dirIterator.fileInfo();
        if (fileInfo.isDir() || fileInfo.isSymLink())
            continue;

        MacBinaryInfo binaryInfo;
        binaryInfo.fileName = fileName;

        // try to find libraries in frameworks
        if (fileName.contains(QLatin1String(".framework/")) && !fileName.contains(QLatin1String("/Headers/"))
            && updateExecutableInfo(&binaryInfo) == 0) {
                patchableBinaries.insert(binaryInfo);
        } else if (fileName.endsWith(QLatin1String(".dylib")) && updateExecutableInfo(&binaryInfo) == 0) {
            patchableBinaries.insert(binaryInfo);
        } // the endsWith checks are here because there might be wrongly committed files in the repositories
        else if (dirIterator.fileInfo().isExecutable() && !fileName.endsWith(QLatin1String(".h"))
            && !fileName.endsWith(QLatin1String(".cpp")) && !fileName.endsWith(QLatin1String(".pro"))
            && !fileName.endsWith(QLatin1String(".pri")) && updateExecutableInfo(&binaryInfo) == 0) {
                 patchableBinaries.insert(binaryInfo);
        }
    }
    return patchableBinaries;
}

bool MacReplaceInstallNamesOperation::apply(const QString &searchString, const QString &replacement,
    const QString &searchDir)
{
    foreach (const MacBinaryInfo &info, collectPatchableBinaries(searchDir))
        relocateBinary(info, searchString, replacement);

    return error() == NoError;
}

int MacReplaceInstallNamesOperation::updateExecutableInfo(MacBinaryInfo *binaryInfo)
{
    QProcessWrapper otool;
    otool.start(QLatin1String("otool"), QStringList() << QLatin1String("-l") << binaryInfo->fileName);
    if (!otool.waitForStarted()) {
        setError(UserDefinedError, tr("Can't invoke otool. Is Xcode installed?"));
        return -1;
    }
    otool.waitForFinished();
    enum State {
        State_Start,
        State_LC_ID_DYLIB,
        State_LC_LOAD_DYLIB
    };
    State state = State_Start;
    QByteArray outputData = otool.readAllStandardOutput();
    if (outputData.contains("is not an object file"))
        return -1;

    QBuffer output(&outputData);
    output.open(QBuffer::ReadOnly);
    while (!output.atEnd()) {
        QString line = QString::fromLocal8Bit(output.readLine()).trimmed();
        if (line.startsWith(QLatin1String("cmd "))) {
            line.remove(0, 4);
            if (line == QLatin1String("LC_LOAD_DYLIB"))
                state = State_LC_LOAD_DYLIB;
            else if (line == QLatin1String("LC_ID_DYLIB"))
                state = State_LC_ID_DYLIB;
            else
                state = State_Start;
        } else if (state != State_Start && line.startsWith(QLatin1String("name "))) {
            line.remove(0, 5);
            int idx = line.indexOf(QLatin1String("(offset"));
            if (idx > 0)
                line.truncate(idx);
            line = line.trimmed();
            if (state == State_LC_LOAD_DYLIB)
                binaryInfo->dependentDynamicLibs.append(line);
            if (state == State_LC_ID_DYLIB)
                binaryInfo->dynamicLibId = line;
        }
    }
    return otool.exitCode();
}

void MacReplaceInstallNamesOperation::relocateBinary(const MacBinaryInfo &info, const QString &searchString,
    const QString &replacement)
{
    qDebug() << QString::fromLatin1("Got the following information(fileName: %1, dynamicLibId: %2, "
        "dynamicLibs: \n\t%3,").arg(info.fileName, info.dynamicLibId, info.dependentDynamicLibs
        .join(QLatin1String("|")));


    // change framework ID only if dynamicLibId isn't only the filename, if it has no relative path ("@")
    if (!info.dynamicLibId.isEmpty() && (info.dynamicLibId != QFileInfo(info.fileName).fileName())
        && !info.dynamicLibId.contains(QLatin1String("@"))) {
            // error is set inside the execCommand method
            if (!execCommand(QLatin1String("install_name_tool"), QStringList(QLatin1String("-id"))
                    << info.fileName << info.fileName)) {
                return;
            }
    }

    QStringList args;
    foreach (const QString &dynamicLib, info.dependentDynamicLibs) {
        if (!dynamicLib.contains(searchString))
            continue;
        args << QLatin1String("-change") << dynamicLib << replacement + dynamicLib.section(searchString, -1);
    }

    // nothing found to patch
    if (args.empty())
        return;

    // error is set inside the execCommand method
    // last argument is the file target which will be patched
    execCommand(QLatin1String("install_name_tool"), args << info.fileName);
}

bool MacReplaceInstallNamesOperation::execCommand(const QString &cmd, const QStringList &args)
{
    qDebug() << Q_FUNC_INFO << cmd << " " << args;

    QProcessWrapper process;
    process.start(cmd, args);
    if (!process.waitForStarted()) {
        setError(UserDefinedError, tr("Can't start process %0.").arg(cmd));
        return false;
    }
    process.waitForFinished();
    if (process.exitCode() != 0) {
        const QString errorMessage = QLatin1String("Command %1 failed.\nArguments: %2\nOutput: %3\n");
        setError(UserDefinedError, errorMessage.arg(cmd, args.join(QLatin1String(" ")),
            QString::fromLocal8Bit(process.readAll())));
        return false;
    }
    return true;
}
