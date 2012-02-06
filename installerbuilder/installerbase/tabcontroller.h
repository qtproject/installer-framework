/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
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
** Nokia at qt-info@nokia.com.
**
**************************************************************************/

#ifndef TABCONTROLLER_H
#define TABCONTROLLER_H

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QInstaller {
    class PackageManagerGui;
    class PackageManagerCore;
    class Settings;
}

class IntroductionPageImpl;

class TabController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TabController)

public:
    explicit TabController(QObject *parent = 0);
    ~TabController();

    void setGui(QInstaller::PackageManagerGui *gui);
    void setManager(QInstaller::PackageManagerCore *core);
    void setManagerParams(const QHash<QString, QString> &params);

    void setControlScript(const QString &script);

public Q_SLOTS:
    int init();

private Q_SLOTS:
    void restartWizard();
    void onSettingsButtonClicked();
    void onCurrentIdChanged(int newId);
    void onNetworkSettingsChanged(const QInstaller::Settings &settings);

private:
    class Private;
    Private *const d;
};

#endif // TABCONTROLLER_H
