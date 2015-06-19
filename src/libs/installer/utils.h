/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef QINSTALLER_UTILS_H
#define QINSTALLER_UTILS_H

#include "installer_global.h"

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

#ifdef Q_OS_WIN
    QString windowsErrorString(int errorCode);
    QString createCommandline(const QString &program, const QStringList &arguments);
#endif

    QStringList INSTALLER_EXPORT localeCandidates(const QString &locale);

    void INSTALLER_EXPORT setVerbose(bool v);
    bool INSTALLER_EXPORT isVerbose();

    INSTALLER_EXPORT std::ostream& operator<<(std::ostream &os, const QString &string);


    class INSTALLER_EXPORT VerboseWriter
    {
    public:
        VerboseWriter();
        ~VerboseWriter();

        static VerboseWriter *instance();

        void appendLine(const QString &msg);
        void setFileName(const QString &fileName);

    private:
        QTextStream stream;
        QBuffer preFileBuffer;
        QString logFileName;
        QString currentDateTimeAsString;
    };

}

#endif // QINSTALLER_UTILS_H
