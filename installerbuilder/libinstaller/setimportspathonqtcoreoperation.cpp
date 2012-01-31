/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
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
#include "setimportspathonqtcoreoperation.h"

#include "qtpatch.h"

#include <QtCore/QByteArrayMatcher>
#include <QtCore/QDir>
#include <QtCore/QDebug>

using namespace QInstaller;

namespace {
    QByteArray getOldValue(const QString &binaryPath)
    {
        QFileInfo fileInfo(binaryPath);

        if (!fileInfo.exists()) {
            qDebug() << QString::fromLatin1("qpatch: warning: file '%1' not found").arg(binaryPath);
            return QByteArray();
        }


        QFile file(binaryPath);
        int readOpenCount = 0;
        while (!file.open(QFile::ReadOnly) && readOpenCount < 20000) {
            ++readOpenCount;
            qApp->processEvents();
        }
        Q_ASSERT(file.isOpen());
        if (!file.isOpen()) {
            qDebug() << QString::fromLatin1("qpatch: warning: file '%1' can not open as ReadOnly.").arg(
                binaryPath);
            qDebug() << file.errorString();
            return QByteArray();
        }

        const QByteArray source = file.readAll();
        file.close();

        int offset = 0;
        QByteArray searchValue("qt_impspath=");
        QByteArrayMatcher byteArrayMatcher(searchValue);
        offset = byteArrayMatcher.indexIn(source, offset);
        Q_ASSERT(offset > 0);
        if (offset == -1)
            return QByteArray();

        int stringEndPosition = offset;
        while(source.at(stringEndPosition++) != '\0') {}
        //after search string till the first \0 it should be our looking for QByteArray
        return source.mid(offset + searchValue.size(), stringEndPosition - offset);
    }
}

SetImportsPathOnQtCoreOperation::SetImportsPathOnQtCoreOperation()
{
    setName(QLatin1String("SetImportsPathOnQtCore"));
}

void SetImportsPathOnQtCoreOperation::backup()
{
}

bool SetImportsPathOnQtCoreOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exact 2 expected.").arg(name())
            .arg(arguments().count()));
        return false;
    }

    const QString qtCoreLibraryDir = args.at(0);
    const QByteArray newValue = QDir::toNativeSeparators(args.at(1)).toUtf8();

    if (255 < newValue.size()) {
        qDebug() << "qpatch: error: newQtDir needs to be less than 255 characters.";
        return false;
    }
    QStringList libraryFiles;
#ifdef Q_OS_WIN
    libraryFiles << QString::fromLatin1("%1/QtCore4.dll").arg(qtCoreLibraryDir);
    libraryFiles << QString::fromLatin1("%1/QtCore4d.dll").arg(qtCoreLibraryDir);
#else
    libraryFiles << qtCoreLibraryDir + QLatin1String("/libQtCore.so");
#endif
    foreach (const QString coreLibrary, libraryFiles) {
        if (QFile::exists(coreLibrary)) {
            QByteArray oldValue(getOldValue(coreLibrary));
            Q_ASSERT(!oldValue.isEmpty());
            oldValue = QByteArray("qt_impspath=%1").replace("%1", oldValue);
            QByteArray adjutedNewValue = QByteArray("qt_impspath=%1").replace("%1", newValue);

            bool isPatched = QtPatch::patchBinaryFile(coreLibrary, oldValue, adjutedNewValue);
            if (!isPatched) {
                qDebug() << "qpatch: warning: could not patched the imports path in" << coreLibrary;
            }
        }
    }

    return true;
}

bool SetImportsPathOnQtCoreOperation::undoOperation()
{
    return true;
}

bool SetImportsPathOnQtCoreOperation::testOperation()
{
    return true;
}

Operation *SetImportsPathOnQtCoreOperation::clone() const
{
    return new SetImportsPathOnQtCoreOperation();
}

