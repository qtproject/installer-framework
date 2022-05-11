/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef LOCALPACKAGEHUB_H
#define LOCALPACKAGEHUB_H

#include "updater.h"

#include <QCoreApplication>
#include <QDate>
#include <QStringList>

namespace KDUpdater {

struct KDTOOLS_EXPORT LocalPackage
{
    QString name;
    QString title;
    QString description;
    int sortingPriority;
    QPair<QString, bool> treeName;
    QString version;
    QString inheritVersionFrom;
    QStringList dependencies;
    QStringList autoDependencies;
    QDate lastUpdateDate;
    QDate installDate;
    bool forcedInstallation;
    bool virtualComp;
    quint64 uncompressedSize;
    bool checkable;
    bool expandedByDefault;
    QString contentSha1;
};

class KDTOOLS_EXPORT LocalPackageHub
{
    Q_DISABLE_COPY(LocalPackageHub)
    Q_DECLARE_TR_FUNCTIONS(LocalPackageHub)

public:
    LocalPackageHub();
    ~LocalPackageHub();

    enum Error
    {
        NoError = 0,
        NotYetReadError,
        CouldNotReadPackageFileError,
        InvalidXmlError,
        InvalidContentError
    };

    bool isValid() const;

    QMap<QString, LocalPackage> localPackages() const;
    QStringList packageNames() const;

    Error error() const;
    QString errorString() const;

    QString fileName() const;
    void setFileName(const QString &fileName);

    QString applicationName() const;
    void setApplicationName(const QString &name);

    QString applicationVersion() const;
    void setApplicationVersion(const QString &version);

    void clearPackageInfos();
    int packageInfoCount() const;

    QList<LocalPackage> packageInfos() const;
    LocalPackage packageInfo(const QString &pkgName) const;

    void addPackage(const QString &pkgName,
                    const QString &version, // mandatory
                    const QString &title,
                    const QPair<QString, bool> &treeName,
                    const QString &description,
                    const int sortingPriority,
                    const QStringList &dependencies,
                    const QStringList &autoDependencies,
                    bool forcedInstallation,
                    bool virtualComp,
                    quint64 uncompressedSize,
                    const QString &inheritVersionFrom,
                    bool checkable,
                    bool expandedByDefault,
                    const QString &contentSha1);
    bool removePackage(const QString &pkgName);

    void refresh();
    void writeToDisk();

private:
    struct PackagesInfoData;
    PackagesInfoData *d;
};

} // KDUpdater

#endif // LOCALPACKAGEHUB_H
