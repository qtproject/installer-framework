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
#include "macreplaceinstallnamesoperation.h"

#include "common/utils.h"
#include "qprocesswrapper.h"

#include <QtCore/QBuffer>
#include <QtCore/QDirIterator>


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
    // 1. indicator to find the original build directory,
    //    means the path till this will be used to replace it with 2.
    // 2. new/current target install directory(the replacement)
    // 3. directory containing frameworks
    // 4. other directory containing frameworks
    // 5. other directory containing frameworks
    // 6. ...

    verbose() << arguments().join(QLatin1String(";")) << std::endl;
    if (arguments().count() < 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, 3 expected.").arg(name())
            .arg(arguments().count()));
        return false;
    }

    QString indicator = arguments().at(0);
    QString installationDir = arguments().at(1);
    QStringList searchDirList = arguments().mid(2);
    foreach (const QString &searchDir, searchDirList) {
        if (!apply(indicator, installationDir, searchDir))
            return false;
    }

    return true;
}

bool MacReplaceInstallNamesOperation::undoOperation()
{
    return true;
}

bool MacReplaceInstallNamesOperation::testOperation()
{
    return true;
}

Operation* MacReplaceInstallNamesOperation::clone() const
{
    return new MacReplaceInstallNamesOperation;
}

bool MacReplaceInstallNamesOperation::apply(const QString &indicator, const QString &installationDir,
    const QString &searchDir)
{
    mIndicator = indicator;
    mInstallationDir = installationDir;

    QDirIterator dirIterator(searchDir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (dirIterator.hasNext()) {
        QString fileName = dirIterator.next();
        if (dirIterator.fileInfo().isDir() && fileName.endsWith(QLatin1String(".framework")))
            relocateFramework(fileName);
        else if (dirIterator.fileInfo().isDir())
            continue;
        else if (fileName.endsWith(QLatin1String(".dylib")))
            relocateBinary(fileName);
        else if (dirIterator.fileInfo().isExecutable() && !fileName.endsWith(QLatin1String(".h"))
            && !fileName.endsWith(QLatin1String(".cpp")) && !fileName.endsWith(QLatin1String(".pro"))
            && !fileName.endsWith(QLatin1String(".pri"))) {
                //the endsWith check are here because there were wrongly commited files in the repositories
                relocateBinary(fileName);
        }
    }

    return error() == NoError;
}

void MacReplaceInstallNamesOperation::extractExecutableInfo(const QString &fileName, QString& frameworkId,
    QStringList &frameworks, QString &originalBuildDir)
{
    verbose() << "Relocator calling otool -l for " << fileName << std::endl;
    QProcessWrapper otool;
    otool.start(QLatin1String("otool"), QStringList() << QLatin1String("-l") << fileName);
    if (!otool.waitForStarted()) {
        setError(UserDefinedError, tr("Can't invoke otool. Is Xcode installed?"));
        return;
    }
    otool.waitForFinished();
    enum State {
        State_Start,
        State_LC_ID_DYLIB,
        State_LC_LOAD_DYLIB
    };
    State state = State_Start;
    QByteArray outputData = otool.readAllStandardOutput();
    QBuffer output(&outputData);
    output.open(QBuffer::ReadOnly);
    while (!output.atEnd()) {
        QString line = QString::fromLocal8Bit(output.readLine());
        line = line.trimmed();
        if (line.startsWith(QLatin1String("cmd "))) {
            line.remove(0, 4);
            if (line == QLatin1String("LC_LOAD_DYLIB"))
                state = State_LC_LOAD_DYLIB;
            else if (line == QLatin1String("LC_ID_DYLIB"))
                state = State_LC_ID_DYLIB;
            else
                state = State_Start;
        } else if (state == State_LC_LOAD_DYLIB && line.startsWith(QLatin1String("name "))) {
            line.remove(0, 5);
            int idx = line.indexOf(QLatin1String("(offset"));
            if (idx > 0)
                line.truncate(idx);
            line = line.trimmed();
            frameworks.append(line);
        } else if (state == State_LC_ID_DYLIB && line.startsWith(QLatin1String("name "))) {
            line.remove(0, 5);
            int idx = line.indexOf(QLatin1String("(offset"));
            if (idx > 0)
                line.truncate(idx);
            line = line.trimmed();
            frameworkId = line;

            originalBuildDir = frameworkId;
            idx = originalBuildDir.indexOf(mIndicator);
            if (idx < 0) {
                originalBuildDir.clear();
            } else {
                originalBuildDir.truncate(idx);
            }
            if (originalBuildDir.endsWith(QLatin1Char('/')))
                originalBuildDir.chop(1);
            verbose() << "originalBuildDir is: " << originalBuildDir << std::endl;
        }
    }
    verbose() << "END - Relocator calling otool -l for " << fileName << std::endl;
}

void MacReplaceInstallNamesOperation::relocateBinary(const QString &fileName)
{
    QString frameworkId;
    QStringList frameworks;
    QString originalBuildDir;
    extractExecutableInfo(fileName, frameworkId, frameworks, originalBuildDir);

    verbose() << "got following informations(fileName, frameworkId, frameworks, orginalBuildDir): " << std::endl;
    verbose() << fileName << ", " << frameworkId << ", " << frameworks.join(QLatin1String("|")) << ", "
        << originalBuildDir << std::endl;

    QStringList args;
    if (frameworkId.contains(mIndicator) || QFileInfo(frameworkId).fileName() == frameworkId) {
        args << QLatin1String("-id") << fileName << fileName;
        if (!execCommand(QLatin1String("install_name_tool"), args))
            return;
    }


    foreach (const QString &fw, frameworks) {
        if (originalBuildDir.isEmpty() && fw.contains(mIndicator)) {
            originalBuildDir = fw.left(fw.indexOf(mIndicator));
        }
        if (originalBuildDir.isEmpty() || !fw.contains(originalBuildDir))
            continue;
        QString newPath = fw;
        newPath.replace(originalBuildDir, mInstallationDir);

        args.clear();
        args << QLatin1String("-change") << fw << newPath << fileName;
        if (!execCommand(QLatin1String("install_name_tool"), args))
            return;
    }
}

void MacReplaceInstallNamesOperation::relocateFramework(const QString &directoryName)
{
    QFileInfo fi(directoryName);
    QString frameworkName = fi.baseName();

    QString absoluteVersionDirectory = directoryName + QLatin1String("/Versions/Current");
    if (QFileInfo(absoluteVersionDirectory).isSymLink()) {
        absoluteVersionDirectory = QFileInfo(absoluteVersionDirectory).symLinkTarget();
    }

    fi.setFile(absoluteVersionDirectory + QDir::separator() + frameworkName);
    if (fi.exists()) {
        QString fileName = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
        relocateBinary(fileName);
    }

    fi.setFile(absoluteVersionDirectory + QDir::separator() + frameworkName + QLatin1String("_debug"));
    if (fi.exists()) {
        QString fileName = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
        relocateBinary(fileName);
    }
}

bool MacReplaceInstallNamesOperation::execCommand(const QString &cmd, const QStringList &args)
{
    verbose() << "Relocator::execCommand " << cmd << " " << args << std::endl;

    QProcessWrapper process;
    process.start(cmd, args);
    if (!process.waitForStarted()) {
        setError(UserDefinedError, tr("Can't start process %0.").arg(cmd));
        return false;
    }
    process.waitForFinished();
    if (process.exitCode() != 0) {
        QString errorMessage = QLatin1String("Command %1 failed.\nArguments: %2\nOutput: %3\n");
        setError(UserDefinedError, errorMessage.arg(cmd, args.join(QLatin1String(" ")),
            QString::fromLocal8Bit(process.readAll())));
        return false;
    }
    return true;
}
