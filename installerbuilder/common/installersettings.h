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
#ifndef INSTALLERSETTINGS_H
#define INSTALLERSETTINGS_H

#include "installer_global.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSharedDataPointer>

namespace QInstaller {
class Repository;

class INSTALLER_EXPORT InstallerSettings
{
    Q_DECLARE_TR_FUNCTIONS(InstallerSettings)

public:
    explicit InstallerSettings();
    ~InstallerSettings();

    InstallerSettings(const InstallerSettings &other);
    InstallerSettings &operator=(const InstallerSettings &other);

    static InstallerSettings fromFileAndPrefix(const QString &path, const QString &prefix);

    bool isValid() const;

    QString logo() const;
    QString logoSmall() const;
    QString title() const;
    QString maintenanceTitle() const;
    QString publisher() const;
    QString url() const;
    QString watermark() const;
    QString background() const;
    QString icon() const;

    QString applicationName() const;
    QString applicationVersion() const;

    QString runProgram() const;
    QString runProgramDescription() const;
    QString startMenuDir() const;
    QString targetDir() const;
    QString adminTargetDir() const;

    QString removeTargetDir() const;
    QString uninstallerName() const;
    QString uninstallerIniFile() const;

    QByteArray privateKey() const;
    QByteArray publicKey() const;

    QString configurationFileName() const;

    QList<Repository> repositories() const;
    void setTemporaryRepositories(const QList<Repository> &repos, bool replace);

    QList<Repository> userRepositories() const;
    void addUserRepositories(const QList<Repository> &repositories);

    QStringList certificateFiles() const;

private:
    class Private;
    QSharedDataPointer<Private> d;
};

}

#endif // INSTALLERSETTINGS_H
