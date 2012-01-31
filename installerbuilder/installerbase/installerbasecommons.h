/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2010, 2012 Nokia Corporation and/or its subsidiary(-ies).
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
#ifndef INSTALLERBASECOMMONS_H
#define INSTALLERBASECOMMONS_H

#include <packagemanagergui.h>

QT_BEGIN_NAMESPACE
class QLabel;
class QString;
class QProgressBar;
QT_END_NAMESPACE


// -- IntroductionPageImpl

class IntroductionPageImpl : public QInstaller::IntroductionPage
{
    Q_OBJECT

public:
    explicit IntroductionPageImpl(QInstaller::PackageManagerCore *core);

    int nextId() const;
    bool validatePage();

    void showAll();
    void hideAll();
    void showMetaInfoUdate();
    void showMaintenanceTools();
    void setMaintenanceToolsEnabled(bool enable);

public Q_SLOTS:
    void onCoreNetworkSettingsChanged();
    void setMessage(const QString &msg);
    void setErrorMessage(const QString &error);

Q_SIGNALS:
    void packageManagerCoreTypeChanged();

private Q_SLOTS:
    void setUpdater(bool value);
    void setUninstaller(bool value);
    void setPackageManager(bool value);

private:
    void entering();
    void leaving();

    void showWidgets(bool show);
    void callControlScript(const QString &callback);

private:
    bool m_updatesFetched;
    bool m_updatesCompleted;
    bool m_allPackagesFetched;

    QLabel *m_label;
    QLabel *m_errorLabel;
    QProgressBar *m_progressBar;
    QRadioButton *m_packageManager;
    QRadioButton *m_updateComponents;
    QRadioButton *m_removeAllComponents;
};


// --TargetDirectoryPageImpl

class TargetDirectoryPageImpl : public QInstaller::TargetDirectoryPage
{
    Q_OBJECT

public:
    explicit TargetDirectoryPageImpl(QInstaller::PackageManagerCore *core);

    QString targetDirWarning() const;
    bool isComplete() const;
    bool askQuestion(const QString &identifier, const QString &message);
    bool failWithError(const QString &identifier, const QString &message);
    bool validatePage();

private:
    QLabel *m_warningLabel;
};


// -- InstallerGui

class InstallerGui : public QInstaller::PackageManagerGui
{
    Q_OBJECT

public:
    explicit InstallerGui(QInstaller::PackageManagerCore *core);

    virtual void init();
    virtual int nextId() const;
};


// -- MaintenanceGui

class MaintenanceGui : public QInstaller::PackageManagerGui
{
    Q_OBJECT

public:
    explicit MaintenanceGui(QInstaller::PackageManagerCore *core);

    virtual void init();
    virtual int nextId() const;

private Q_SLOTS:
    void updateRestartPage();
};

#endif // INSTALLERBASECOMMONS_H
