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
#include "setqtcreatorvalueoperation.h"

#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QSettings>
#include <QDebug>

using namespace QInstaller;

#if defined ( Q_OS_MAC )
    static const char *QtCreatorSettingsSuffixPath =
        "/Qt Creator.app/Contents/Resources/Nokia/QtCreator.ini";
#else
    static const char *QtCreatorSettingsSuffixPath =
        "/QtCreator/share/qtcreator/Nokia/QtCreator.ini";
#endif

SetQtCreatorValueOperation::SetQtCreatorValueOperation()
{
    setName(QLatin1String("SetQtCreatorValue"));
}

SetQtCreatorValueOperation::~SetQtCreatorValueOperation()
{
}

void SetQtCreatorValueOperation::backup()
{
}

namespace {
    QString groupName(const QString & groupName)
    {
        if(groupName == QLatin1String("General")) {
            return QString();
        } else {
            return groupName;
        }
    }
}

bool SetQtCreatorValueOperation::performOperation()
{
    const QStringList args = arguments();

    if( args.count() != 4) {
        setError( InvalidArguments );
        setErrorString( tr("Invalid arguments in %0: %1 arguments given, exact 4 expected(rootInstallPath,group,key,value).")
                        .arg(name()).arg( arguments().count() ) );
        return false;
    }

    const QString &rootInstallPath = args.at(0); //for example "C:\\Nokia_SDK\\"

    const QString &group = groupName(args.at(1));
    const QString &key = args.at(2);
    const QString &value = args.at(3);

    QSettings settings(rootInstallPath + QLatin1String(QtCreatorSettingsSuffixPath),
                        QSettings::IniFormat);

    if(!group.isEmpty()) {
        settings.beginGroup(group);
    }

    settings.setValue(key, value);

    if(!group.isEmpty()) {
        settings.endGroup();
    }

    return true;
}

bool SetQtCreatorValueOperation::undoOperation()
{
    return true;
}

bool SetQtCreatorValueOperation::testOperation()
{
    return true;
}

KDUpdater::UpdateOperation* SetQtCreatorValueOperation::clone() const
{
    return new SetQtCreatorValueOperation();
}

