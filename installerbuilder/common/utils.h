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

    QByteArray INSTALLER_EXPORT calculateHash(QIODevice *device, QCryptographicHash::Algorithm algo);

    QString INSTALLER_EXPORT replaceVariables(const QHash<QString,QString> &vars, const QString &str);
    QString INSTALLER_EXPORT replaceWindowsEnvironmentVariables(const QString &str);
    QStringList INSTALLER_EXPORT parseCommandLineArgs(int argc, char **argv);
#ifdef Q_OS_WIN
    QString createCommandline(const QString &program, const QStringList &arguments);
#endif

    void INSTALLER_EXPORT setVerbose(bool v);
    bool INSTALLER_EXPORT isVerbose();

    INSTALLER_EXPORT std::ostream& stdverbose();
    INSTALLER_EXPORT std::ostream& operator<<(std::ostream &os, const QString &string);

    class VerboseWriter;
    INSTALLER_EXPORT VerboseWriter &verbose();

    class INSTALLER_EXPORT VerboseWriter : public QObject
    {
        Q_OBJECT
    public:
        VerboseWriter(QObject *parent = 0);
        ~VerboseWriter();

        static VerboseWriter *instance();

        inline VerboseWriter &operator<<(const char *t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(std::ostream& (*f)(std::ostream &s)) { stdverbose() << *f; stream << "\n"; return *this; }
    public slots:
        void setOutputStream(const QString &fileName);

    private:
        QTextStream stream;
        QBuffer preFileBuffer;
        QString logFileName;
    };

}

#endif // QINSTALLER_UTILS_H
