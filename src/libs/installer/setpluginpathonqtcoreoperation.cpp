/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
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

#include "setpluginpathonqtcoreoperation.h"

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
        QByteArray searchValue("qt_plugpath=");
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

SetPluginPathOnQtCoreOperation::SetPluginPathOnQtCoreOperation()
{
    setName(QLatin1String("SetPluginPathOnQtCore"));
}

void SetPluginPathOnQtCoreOperation::backup()
{
}

bool SetPluginPathOnQtCoreOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exactly 2 expected.").arg(name())
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
    foreach (const QString &coreLibrary, libraryFiles) {
        if (QFile::exists(coreLibrary)) {
            QByteArray oldValue(getOldValue(coreLibrary));
            Q_ASSERT(!oldValue.isEmpty());
            oldValue = QByteArray("qt_plugpath=%1").replace("%1", oldValue);
            QByteArray adjutedNewValue = QByteArray("qt_plugpath=%1").replace("%1", newValue);

            bool isPatched = QtPatch::patchBinaryFile(coreLibrary, oldValue, adjutedNewValue);
            if (!isPatched)
                qDebug() << "qpatch: warning: could not patch the plugin path in" << coreLibrary;
        }
    }

    return true;
}

bool SetPluginPathOnQtCoreOperation::undoOperation()
{
    return true;
}

bool SetPluginPathOnQtCoreOperation::testOperation()
{
    return true;
}

Operation *SetPluginPathOnQtCoreOperation::clone() const
{
    return new SetPluginPathOnQtCoreOperation();
}

