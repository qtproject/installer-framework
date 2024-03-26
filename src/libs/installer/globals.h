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
#ifndef GLOBALS_H
#define GLOBALS_H

#include "installer_global.h"

#include <QRegularExpression>
#include <QLoggingCategory>

namespace QInstaller {

INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcServer)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcInstallerInstallLog)

INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcProgressIndicator)
INSTALLER_EXPORT Q_DECLARE_LOGGING_CATEGORY(lcDeveloperBuild)

QStringList INSTALLER_EXPORT loggingCategories();

QRegularExpression INSTALLER_EXPORT commaRegExp();
inline QStringList splitStringWithComma(const QString &value) {
    if (!value.isEmpty())
        return value.split(QInstaller::commaRegExp(), Qt::SkipEmptyParts);
    return QStringList();
}

QString htmlToString(const QString &html);
QString enumToString(const QMetaObject& metaObject, const char *enumerator, int key);

template <typename T, template<typename> typename C>
QSet<T> toQSet(const C<T> &container)
{
    return QSet<T>(container.begin(), container.end());
}

void askForCredentials(QString *username, QString *password, const QString &usernameTitle, const QString &passwordTitle);

}   // QInstaller

#endif  // GLOBALS_H
