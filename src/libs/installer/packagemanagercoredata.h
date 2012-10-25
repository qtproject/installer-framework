/****************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of Installer Framework.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
****************************************************************************/

#ifndef PACKAGEMANAGERCOREDATA_H
#define PACKAGEMANAGERCOREDATA_H

#include "settings.h"

namespace QInstaller {

class PackageManagerCoreData
{
public:
    PackageManagerCoreData() {}
    explicit PackageManagerCoreData(const QHash<QString, QString> &variables);

    void clear();

    Settings &settings() const;
    QList<QString> keys() const;

    bool contains(const QString &key) const;
    bool setValue(const QString &key, const QString &normalizedValue);
    QVariant value(const QString &key, const QVariant &_default = QVariant()) const;

    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &ba) const;

private:
    mutable Settings m_settings;
    QHash<QString, QString> m_variables;
};

}   // namespace QInstaller

#endif  // PACKAGEMANAGERCOREDATA_H
