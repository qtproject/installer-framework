/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#ifndef PRODUCTKEYCHECK_H
#define PRODUCTKEYCHECK_H

#include "installer_global.h"

#include <QString>
#include <QUiLoader>

namespace QInstaller {

class PackageManagerCore;
class Repository;

} // QInstaller

class INSTALLER_EXPORT ProductKeyCheck
{
    Q_DISABLE_COPY(ProductKeyCheck)

public:
    ~ProductKeyCheck();
    static ProductKeyCheck *instance();
    void init(QInstaller::PackageManagerCore *core);

    static QUiLoader *uiLoader();

    // was validLicense
    bool hasValidKey();
    QString lastErrorString();
    QString maintainanceToolDetailErrorNotice();

    bool applyKey(const QStringList &arguments);

    // to filter none valid licenses
    bool isValidLicenseTextFile(const QString &fileName);

    // to filter repositories not matching the license
    bool isValidRepository(const QInstaller::Repository &repository) const;

    void addPackagesFromXml(const QString &xmlPath);
    bool isValidPackage(const QString &packageName) const;

    QList<int> registeredPages() const;
    bool hasValidLicense() const;
    bool hasAcceptedAllLicenses() const;
    QString licenseAcceptanceText() const;
    QString securityWarning() const;
    QString additionalMetaDownloadWarning() const;

private:
    ProductKeyCheck();
    class ProductKeyCheckPrivate *const d;
};

#endif // PRODUCTKEYCHECK_H
