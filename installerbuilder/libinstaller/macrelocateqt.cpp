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
#include "macrelocateqt.h"
#include "common/utils.h"
#include "fsengineclient.h"
#include "qprocesswrapper.h"

#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <QtCore/QBuffer>
#include <QtCore/QProcess>

using namespace QInstaller;

Relocator::Relocator()
{
}

bool Relocator::apply(const QString &qtInstallDir, const QString &targetDir)
{
    verbose() << "Relocator::apply(" << qtInstallDir << ')' << std::endl;

    mErrorMessage.clear();
    mOriginalInstallDir.clear();

    {
        QFile buildRootFile(qtInstallDir + QLatin1String("/.orig_build_root"));
        if (buildRootFile.exists() && buildRootFile.open(QFile::ReadOnly)) {
            mOriginalInstallDir = QString::fromLocal8Bit(buildRootFile.readAll()).trimmed();
            if (!mOriginalInstallDir.endsWith(QLatin1Char('/')))
                mOriginalInstallDir += QLatin1Char('/');
        }
    }

    mInstallDir = targetDir;
    if (!mInstallDir.endsWith(QLatin1Char('/')))
        mInstallDir.append(QLatin1Char('/'));
    if (!QFile::exists(qtInstallDir + QLatin1String("/bin/qmake"))) {
        mErrorMessage = QLatin1String("This is not a Qt installation directory.");
        return false;
    }

    {
        QDirIterator dirIterator(qtInstallDir + QLatin1String("/lib"), QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (dirIterator.hasNext()) {
            QString dirName = dirIterator.next();
            if (dirName.endsWith(QLatin1String(".framework")))
                relocateFramework(dirName);
        }
    }

    QStringList dyLibDirs;
    dyLibDirs << QLatin1String("/plugins") << QLatin1String("/lib") << QLatin1String("/imports");
    foreach (QString dylibItem, dyLibDirs){
        QDirIterator dirIterator(qtInstallDir + dylibItem, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (dirIterator.hasNext()) {
            QString fileName = dirIterator.next();
            if (fileName.endsWith(QLatin1String(".dylib"))) {
                relocateBinary(fileName);
            }
        }
    }

    // We should not iterate over each file, but to be sure check each of those in relocate
    {
        QDirIterator dirIterator(qtInstallDir + QLatin1String("/bin"), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (dirIterator.hasNext()) {
            QString fileName = dirIterator.next();
            if (fileName.contains(QLatin1String("app/Contents")) && !fileName.contains(QLatin1String("/MacOS")))
                continue;
            relocateBinary(fileName);
        }
    }

    return mErrorMessage.isNull();
}

bool Relocator::containsOriginalBuildDir(const QString &dirName)
{
    int idx = dirName.indexOf(QLatin1String("_BUILD_"));
    if (idx < 0)
        return false;
    return dirName.indexOf(QLatin1String("_PADDED_"), idx) >= 0;
}

void Relocator::extractExecutableInfo(const QString& fileName, QStringList& frameworks)
{
    verbose() << "Relocator calling otool -l for " << fileName << std::endl;
    QProcess otool;
    otool.start(QLatin1String("otool"), QStringList() << QLatin1String("-l") << fileName);
    if (!otool.waitForStarted()) {
        mErrorMessage = QLatin1String("Can't start otool. Is Xcode installed?");
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
//        qDebug() << line;
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
            if (containsOriginalBuildDir(line))
                frameworks.append(line);
        } else if (state == State_LC_ID_DYLIB && mOriginalInstallDir.isNull() && line.startsWith(QLatin1String("name "))) {
            line.remove(0, 5);
            if (containsOriginalBuildDir(line)) {
                mOriginalInstallDir = line;
                const QString lastBuildDirPart = QLatin1String("/ndk/");
                int idx = mOriginalInstallDir.indexOf(lastBuildDirPart);
                if (idx < 0)
                    continue;
                mOriginalInstallDir.truncate(idx + lastBuildDirPart.length());
            }
        }
    }
}

void Relocator::relocateBinary(const QString& fileName)
{
    QStringList frameworks;
    extractExecutableInfo(fileName, frameworks);

    QStringList args;
    args << QLatin1String("-id") << fileName << fileName;
    if (!execCommand(QLatin1String("install_name_tool"), args))
        return;

    foreach (const QString& fw, frameworks) {
        if (!fw.startsWith(mOriginalInstallDir))
            continue;

        QString newPath = mInstallDir;
        newPath += fw.mid(mOriginalInstallDir.length());

        args.clear();
        args << QLatin1String("-change") << fw << newPath << fileName;
        if (!execCommand(QLatin1String("install_name_tool"), args))
            return;
    }
}

void Relocator::relocateFramework(const QString& directoryName)
{
    QFileInfo fi(directoryName);
    QString frameworkName = fi.baseName();
    fi.setFile(directoryName + QLatin1String("/Versions/Current/") + frameworkName);
    if (fi.exists()) {
        QString fileName = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
        relocateBinary(fileName);
    }
    fi.setFile(directoryName + QLatin1String("/Versions/Current/") + frameworkName + QLatin1String("_debug"));
    if (fi.exists()) {
        QString fileName = fi.isSymLink() ? fi.symLinkTarget() : fi.absoluteFilePath();
        relocateBinary(fileName);
    }
}

bool Relocator::execCommand(const QString& cmd, const QStringList& args)
{
    verbose() << "Relocator::execCommand " << cmd << " " << args << std::endl;
    QProcessWrapper process;
    process.start(cmd, args);
    if (!process.waitForStarted()) {
        mErrorMessage = QLatin1String("Can't start process ") + cmd + QLatin1String(".");
        return false;
    }
    process.waitForFinished();
    if (process.exitCode() != 0) {
        mErrorMessage = QLatin1String("Command %1 failed.\nArguments: %2\nOutput: %3\n");
        mErrorMessage = mErrorMessage.arg(cmd, args.join(QLatin1String(" ")), QString::fromLocal8Bit(process.readAll()));
        return false;
    }
    return true;
}
