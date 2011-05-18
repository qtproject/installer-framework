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
#ifndef QINSTALLER_UTILS_H
#define QINSTALLER_UTILS_H

#include <QtCore/QCryptographicHash>
#include <QtCore/QFile>
#include <QtCore/QTextStream>
#include <QtCore/QBuffer>
#include <QtCore/QUrl>

#include "installer_global.h"

#include <ostream>

class QByteArray;
class QIODevice;
class QString;
class QUrl;

template< typename T >
class QList;

template<typename K, typename V> class QHash;

namespace QInstaller
{
    QByteArray INSTALLER_EXPORT calculateHash( QIODevice* device, QCryptographicHash::Algorithm algo );

    QString INSTALLER_EXPORT replaceVariables( const QHash<QString,QString>& vars, const QString &str );
    QString INSTALLER_EXPORT replaceWindowsEnvironmentVariables( const QString &str );
    QStringList INSTALLER_EXPORT parseCommandLineArgs(int argc, char **argv);

    void INSTALLER_EXPORT setVerbose( bool v );
    bool INSTALLER_EXPORT isVerbose();

    INSTALLER_EXPORT std::ostream& stdverbose();


    INSTALLER_EXPORT std::ostream& operator<<( std::ostream& os, const QUrl& url );
    INSTALLER_EXPORT std::ostream& operator<<( std::ostream& os, const QString& string );
    INSTALLER_EXPORT std::ostream& operator<<( std::ostream& os, const QByteArray& array );
    template< typename T >
    std::ostream& operator<<( std::ostream& os, const QList< T >& list )
    {
        os << "(";
        for( typename QList< T >::const_iterator it = list.begin(); it != list.end(); ++it )
            os << *it;
        os << ");";
        return os;
    }

    class VerboseWriter;
    INSTALLER_EXPORT VerboseWriter& verbose();

    class INSTALLER_EXPORT VerboseWriter : public QObject
    {
        Q_OBJECT
    public:
        VerboseWriter(QObject* parent = 0);
        ~VerboseWriter();

        static VerboseWriter* instance();

        inline VerboseWriter &operator<<(bool t) { stdverbose() << t; stream << (t ? "true" : "false"); return *this; }
        inline VerboseWriter &operator<<(int t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(qint64 t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(quint64 t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(double t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(std::string &t) { stdverbose() << t; stream << QString::fromStdString(t); return *this; }
        inline VerboseWriter &operator<<(const QByteArray &t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(const QString &t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(const QLatin1String &t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(const char *t) { stdverbose() << t; stream << t; return *this; }
        inline VerboseWriter &operator<<(const QUrl &t) { return verbose() << t.toString(); }
        template< typename T >
        VerboseWriter& operator<<(const QList< T >& list )
        {
            verbose() << "List ( ";
            for( typename QList< T >::const_iterator it = list.begin(); it != list.end(); ++it )
                verbose() << *it <<"; ";
            return verbose() << ");";
        }

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
