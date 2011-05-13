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
#include "fsengineclient.h"
#include "qprocesswrapper.h"

#include <QtCore/QDirIterator>
#include <QtCore/QDebug>
#include <QtCore/QBuffer>
#include <QtCore/QProcess>

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
    // 1. indicator to find the original build directory
    // 2. new build directory
    // 3. directory containing frameworks

    if( arguments().count() != 3 ) {
        setError( InvalidArguments );
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, 3 expected.")
                        .arg(name()).arg( arguments().count() ) );
        return false;
    }

    QString indicator = arguments().at(0);
    QString installationDir = arguments().at(1);
    QString searchDir = arguments().at(2);
    return apply(indicator, installationDir, searchDir);
}

bool MacReplaceInstallNamesOperation::undoOperation()
{
    return true;
}

bool MacReplaceInstallNamesOperation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* MacReplaceInstallNamesOperation::clone() const
{
    return new MacReplaceInstallNamesOperation;
}

bool MacReplaceInstallNamesOperation::apply(const QString& indicator, const QString& installationDir, const QString& searchDir)
{
    mOriginalBuildDir.clear();
    mIndicator = indicator;
    mInstallationDir = installationDir;

    {
        QDirIterator dirIterator(searchDir, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (dirIterator.hasNext()) {
            QString dirName = dirIterator.next();
            if (dirName.endsWith(QLatin1String(".framework")))
                relocateFramework(dirName);
        }
    }

    return error() == NoError;
}

void MacReplaceInstallNamesOperation::extractExecutableInfo(const QString& fileName, QString& frameworkId, QStringList& frameworks)
{
    QProcess otool;
    otool.start(QLatin1String("otool"), QStringList() << QLatin1String("-l") << fileName);
    if (!otool.waitForStarted()) {
        setError(UserDefinedError, tr("Can't invoke otool."));
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
            frameworks.append(line);
        } else if (state == State_LC_ID_DYLIB && line.startsWith(QLatin1String("name "))) {
            line.remove(0, 5);
            int idx = line.indexOf(QLatin1String("(offset"));
            if (idx > 0)
                line.truncate(idx);
            line = line.trimmed();
            frameworkId = line;

            mOriginalBuildDir = frameworkId;
            idx = mOriginalBuildDir.indexOf(mIndicator);
            if (idx < 0) {
                mOriginalBuildDir.clear();
            } else {
                mOriginalBuildDir.truncate(idx);
            }
            if (mOriginalBuildDir.endsWith(QLatin1Char('/')))
                mOriginalBuildDir.chop(1);
        }
    }
}

void MacReplaceInstallNamesOperation::relocateBinary(const QString& fileName)
{
    QString frameworkId;
    QStringList frameworks;
    extractExecutableInfo(fileName, frameworkId, frameworks);

    QStringList args;
    if (frameworkId.contains(mIndicator)) {
        args << QLatin1String("-id") << fileName << fileName;
        execCommand(QLatin1String("install_name_tool"), args);
    }

    foreach (const QString& fw, frameworks) {
        if (!fw.contains(mOriginalBuildDir))
            continue;

        QString newPath = fw;
        newPath.replace(mOriginalBuildDir, mInstallationDir);

        args.clear();
        args << QLatin1String("-change") << fw << newPath << fileName;
        execCommand(QLatin1String("install_name_tool"), args);
    }
}

void MacReplaceInstallNamesOperation::relocateFramework(const QString& directoryName)
{
    //qDebug() << "relocateFramework" << directoryName;
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

bool MacReplaceInstallNamesOperation::execCommand(const QString& cmd, const QStringList& args)
{
    //qDebug() << cmd << args;

    QProcessWrapper process;
    process.start(cmd, args);
    if (!process.waitForStarted()) {
        setError(UserDefinedError, tr("Can't start process %0.").arg(cmd));
        return false;
    }
    process.waitForFinished();
    return true;
}
