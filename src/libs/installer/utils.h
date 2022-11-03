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

#ifndef QINSTALLER_UTILS_H
#define QINSTALLER_UTILS_H

#include "installer_global.h"
#include "commandlineparser.h"

#include <QtCore/QBuffer>
#include <QtCore/QCryptographicHash>
#include <QtCore/QHash>
#include <QtCore/QUrl>
#include <QtCore/QTextStream>

#include <ostream>

QT_BEGIN_NAMESPACE
class QIODevice;
QT_END_NAMESPACE

namespace QInstaller {

    void INSTALLER_EXPORT uiDetachedWait(int ms);
    bool INSTALLER_EXPORT startDetached(const QString &program, const QStringList &arguments,
        const QString &workingDirectory, qint64 *pid = 0);

    QByteArray INSTALLER_EXPORT calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo);
    QByteArray INSTALLER_EXPORT calculateHash(const QString &path, QCryptographicHash::Algorithm algo);

    QString INSTALLER_EXPORT replaceVariables(const QHash<QString,QString> &vars, const QString &str);
    QString INSTALLER_EXPORT replaceWindowsEnvironmentVariables(const QString &str);
    QStringList INSTALLER_EXPORT parseCommandLineArgs(int argc, char **argv);

    bool INSTALLER_EXPORT canCreateSymbolicLinks();

#ifdef Q_OS_WIN
    QString windowsErrorString(int errorCode);
    QString createCommandline(const QString &program, const QStringList &arguments);
    bool setPrivilege(const wchar_t *privilege, bool enable);
    bool checkPrivilege(const wchar_t *privilege);
    bool developerModeEnabled();
#endif

    QStringList INSTALLER_EXPORT localeCandidates(const QString &locale);

    QStringList INSTALLER_EXPORT checkMutualOptions(const CommandLineParser &parser, const QStringList &options);

    INSTALLER_EXPORT std::ostream& operator<<(std::ostream &os, const QString &string);
}

#endif // QINSTALLER_UTILS_H
