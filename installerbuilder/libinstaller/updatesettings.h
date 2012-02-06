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

#ifndef UPDATESETTINGS_H
#define UPDATESETTINGS_H

#include "installer_global.h"

QT_BEGIN_NAMESPACE
class QDateTime;
template<typename T>
class QSet;
class QSettings;
QT_END_NAMESPACE

namespace QInstaller {

class Repository;

class INSTALLER_EXPORT UpdateSettings
{
public:
    UpdateSettings();
    ~UpdateSettings();

    enum Interval {
        Daily = 86400,
        Weekly = Daily * 7,
        Monthly = Daily * 30
    };

    static void setSettingsSource(QSettings *settings);

    int updateInterval() const;
    void setUpdateInterval(int seconds);

    QString lastResult() const;
    void setLastResult(const QString &lastResult);

    QDateTime lastCheck() const;
    void setLastCheck(const QDateTime &lastCheck);

    bool checkOnlyImportantUpdates() const;
    void setCheckOnlyImportantUpdates(bool checkOnlyImportantUpdates);

    QSet<Repository> repositories() const;
    void setRepositories(const QSet<Repository> &repositories);

private:
    class Private;
    Private *const d;
};

}

#endif
