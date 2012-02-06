/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (info@qt.nokia.com)
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
** Nokia at info@qt.nokia.com.
**
**************************************************************************/

#include "macreplaceinstallnamesoperation.h"

#include "qprocesswrapper.h"

#include <QtCore/QBuffer>
#include <QtCore/QDebug>
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

Operation *MacReplaceInstallNamesOperation::clone() const
{
    return new MacReplaceInstallNamesOperation;
}

bool MacReplaceInstallNamesOperation::apply(const QString &indicator, const QString &installationDir,
    const QString &searchDir)
{
    m_indicator = indicator;
    m_installationDir = installationDir;

    QStringList alreadyPatchedFrameworks;
    QLatin1String frameworkSuffix(".framework");

    QDirIterator dirIterator(searchDir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (dirIterator.hasNext()) {
        QString fileName = dirIterator.next();

        //check that we don't do anything for already patched framework pathes
        if (fileName.contains(frameworkSuffix)) {
            QString alreadyPatchedSearchString = fileName.left(fileName.lastIndexOf(frameworkSuffix))
                + frameworkSuffix;
            if (alreadyPatchedFrameworks.contains(alreadyPatchedSearchString)) {
                continue;
            }
        }
        if (dirIterator.fileInfo().isDir() && fileName.endsWith(frameworkSuffix)) {
            relocateFramework(fileName);
            alreadyPatchedFrameworks.append(fileName);
        }
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

void MacReplaceInstallNamesOperation::extractExecutableInfo(const QString &fileName, QString &frameworkId,
    QStringList &frameworks, QString &originalBuildDir)
{
    qDebug() << "Relocator calling otool -l for" << fileName;
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
            idx = originalBuildDir.indexOf(m_indicator);
            if (idx < 0) {
                originalBuildDir.clear();
            } else {
                originalBuildDir.truncate(idx);
            }
            if (originalBuildDir.endsWith(QLatin1Char('/')))
                originalBuildDir.chop(1);
            qDebug() << "originalBuildDir is:" << originalBuildDir;
        }
    }
    qDebug() << "END - Relocator calling otool -l for" << fileName;
}

void MacReplaceInstallNamesOperation::relocateBinary(const QString &fileName)
{
    QString frameworkId;
    QStringList frameworks;
    QString originalBuildDir;
    extractExecutableInfo(fileName, frameworkId, frameworks, originalBuildDir);

    qDebug() << QString::fromLatin1("got following informations(fileName: %1, frameworkId: %2, frameworks: %3,"
        "orginalBuildDir: %4)").arg(fileName, frameworkId, frameworks.join(QLatin1String("|")), originalBuildDir);

    QStringList args;
    if (frameworkId.contains(m_indicator) || QFileInfo(frameworkId).fileName() == frameworkId) {
        args << QLatin1String("-id") << fileName << fileName;
        if (!execCommand(QLatin1String("install_name_tool"), args))
            return;
    }


    foreach (const QString &fw, frameworks) {
        if (originalBuildDir.isEmpty() && fw.contains(m_indicator)) {
            originalBuildDir = fw.left(fw.indexOf(m_indicator));
        }
        if (originalBuildDir.isEmpty() || !fw.contains(originalBuildDir))
            continue;
        QString newPath = fw;
        newPath.replace(originalBuildDir, m_installationDir);

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
    qDebug() << Q_FUNC_INFO << cmd << " " << args;

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
