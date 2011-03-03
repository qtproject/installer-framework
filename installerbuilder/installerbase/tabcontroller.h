/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
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
#ifndef TABCONTROLLER_H
#define TABCONTROLLER_H

#include <QtCore/QHash>
#include <QtCore/QObject>

namespace QInstaller {
    class Installer;
    class Gui;
}

namespace KDUpdater {
    class Application;
}

class MainTabWidget;

class TabController : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TabController)

public:
    enum Tabs {
        UPDATER_TAB,
        PACKAGE_MANAGER_TAB
    };

    enum Status {
        SUCCESS = 0,
        CANCELED = 1,
        FAILED = 2
    };

    explicit TabController(QObject *parent = 0);
    ~TabController();

    int init();
    int checkRepositories();
    void setInstaller(QInstaller::Installer *installer);
    void setTabWidget(MainTabWidget *widget);
    void setApplication(KDUpdater::Application *app);
    void setInstallerGui(QInstaller::Gui *gui);
    void setControlScript(const QString &script);
    void setInstallerParams(const QHash<QString, QString> &params);
    Status getState() const;
    Q_INVOKABLE void setCurrentTab(int tab);

private Q_SLOTS:
    void changeCurrentTab(int index);
    void canceled();
    void finished();
    void restartWizard();
    void disableUpdaterTab();
    void enableUpdaterTab();
    void updaterFinished(int val = 0);
    void updaterFinished(bool error);
    void updaterFinishedWithError();
    void close();

    int initUpdater();
    int initPackageManager();

private:
    int initUninstaller();

private:
    class Private;
    Private *const d;
};

#endif // TABCONTROLLER_H
