/****************************************************************************
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
****************************************************************************/

#ifndef PACKAGEMANAGERCOREDATA_H
#define PACKAGEMANAGERCOREDATA_H

#include "settings.h"
#include <QSettings>

namespace QInstaller {

class PackageManagerCoreData
{
public:
    PackageManagerCoreData() {}
    explicit PackageManagerCoreData(const QHash<QString, QString> &variables, const bool isInstaller);

    void clear();
    void addDynamicPredefinedVariables();
    void setUserDefinedVariables(const QHash<QString, QString> &variables);
    void addNewVariable(const QString &key, const QString &value);

    Settings &settings() const;
    QStringList keys() const;

    inline QString settingsFilePath() {
        return m_settingsFilePath;
    }

    bool contains(const QString &key) const;
    bool setValue(const QString &key, const QString &normalizedValue);
    QVariant value(const QString &key, const QVariant &_default = QVariant(), const QSettings::Format &format = QSettings::NativeFormat) const;
    QString key(const QString &value) const;

    QString replaceVariables(const QString &str) const;
    QByteArray replaceVariables(const QByteArray &ba) const;

private:
    mutable Settings m_settings;
    QString m_settingsFilePath;
    QHash<QString, QString> m_variables;
};

}   // namespace QInstaller

#endif  // PACKAGEMANAGERCOREDATA_H
