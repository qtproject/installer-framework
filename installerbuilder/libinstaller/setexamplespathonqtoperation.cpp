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
#include "setexamplespathonqtoperation.h"

#include "qtpatch.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>
#include <QtCore/QDebug>

using namespace QInstaller;

SetExamplesPathOnQtOperation::SetExamplesPathOnQtOperation()
{
    setName(QLatin1String("SetExamplesPathOnQt"));
}

void SetExamplesPathOnQtOperation::backup()
{
}

bool SetExamplesPathOnQtOperation::performOperation()
{
    const QStringList args = arguments();

    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, exact 2 expected.").arg(name())
            .arg(arguments().count()));
        return false;
    }

    const QString qtDir = args.at(0);
    QByteArray newValue = QDir::toNativeSeparators(args.at(1)).toUtf8();

    QString qmakePath = qtDir + QLatin1String("/bin/qmake");
#ifdef Q_OS_WIN
    qmakePath = qmakePath + QLatin1String(".exe");
#endif

    QByteArray qmakeOutput;
    QHash<QString, QByteArray> qmakeValueHash = QtPatch::qmakeValues(qmakePath, &qmakeOutput);

    if (qmakeValueHash.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("The output of \n%1 -query\n is not parseable. Please file a bugreport with this "
            "dialog http://bugreports.qt.nokia.com.\noutput: \"%2\"").arg(QDir::toNativeSeparators(qmakePath),
            QString::fromUtf8(qmakeOutput)));
        return false;
    }

    QByteArray oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_EXAMPLES"));
    bool oldQtPathFromQMakeIsEmpty = oldValue.isEmpty();
    if (oldQtPathFromQMakeIsEmpty) {
        qDebug() << "qpatch: warning: It was not able to get the old values from" << qmakePath;
    }

    if (255 < newValue.size()) {
        setError(UserDefinedError);
        setErrorString(tr("Qt patch error: new Qt example path(%1)\n needs to be less than 255 characters.")
            .arg(QString::fromLocal8Bit(newValue)));
        return false;
    }

    QString qtConfPath = qtDir + QLatin1String("/bin/qt.conf");

    if (QFile::exists(qtConfPath)) {
        QSettings settings(qtConfPath, QSettings::IniFormat);
        settings.setValue( QLatin1String("Paths/Examples"), QString::fromUtf8(newValue));
    }

    oldValue = QByteArray("qt_xmplpath=%1").replace("%1", oldValue);
    newValue = QByteArray("qt_xmplpath=%1").replace("%1", newValue);

    bool isPatched = QtPatch::patchBinaryFile(qmakePath, oldValue, newValue);
    if (!isPatched) {
        qDebug() << "qpatch: warning: could not patched the example path in" << qmakePath;
    }

    return true;
}

bool SetExamplesPathOnQtOperation::undoOperation()
{
    return true;
}

bool SetExamplesPathOnQtOperation::testOperation()
{
    return true;
}

Operation *SetExamplesPathOnQtOperation::clone() const
{
    return new SetExamplesPathOnQtOperation();
}

