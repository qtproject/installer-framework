/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
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

    void setControlScript(const QString &script);

public Q_SLOTS:
    int init();

private Q_SLOTS:
    void restartWizard();
    void onSettingsButtonClicked();
    void onAboutApplicationClicked();
    void onClearCacheClicked();
    void onCurrentIdChanged(int newId);
    void onNetworkSettingsChanged(const QInstaller::Settings &settings);

private:
    class Private;
    Private *const d;
};

#endif // TABCONTROLLER_H
