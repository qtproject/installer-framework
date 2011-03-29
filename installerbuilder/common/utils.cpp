/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
#include "utils.h"

#include <QByteArray>
#include <QHash>
#include <QIODevice>
#include <QProcessEnvironment>
#include <QtCore/QDateTime>
#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QDir>

#include <fstream>
#include <iostream>
#include <sstream>

static bool verb = false;

void QInstaller::setVerbose( bool v ) {
    verb = v;
}

bool QInstaller::isVerbose()
{
    return verb;
}

#ifdef Q_WS_WIN
void qWinMsgHandler(QtMsgType t, const char* str);

class debugstream : public std::ostream
{
    class buf : public std::stringbuf
    {
    public:
        buf()
        {
        }
        int sync()
        {
            std::string s = str();
            if( s[ s.length() - 1 ] == '\n' )
                s[ s.length() - 1 ] = '\0'; // remove \n
            qWinMsgHandler( QtDebugMsg, s.c_str() );
            std::cout << s << std::endl;
            str( std::string() );
            return 0;
        }
    };
public:
    debugstream()
        : std::ostream( &b )
    {
    }
private:
    buf b;
};
#endif

std::ostream& QInstaller::stdverbose()
{
    static std::fstream null;
#ifdef Q_WS_WIN
    // TODO: this one get leaked
    static debugstream& stream = *(new debugstream);
#else
    static std::ostream& stream = std::cout;
#endif
    if( verb )
        return stream;
    else
        return null;
}

std::ostream& QInstaller::operator<<( std::ostream& os, const QUrl& url )
{
    return os << "QUrl( " << url.toString() << ")";
}

std::ostream& QInstaller::operator<<( std::ostream& os, const QString& string )
{
    return os << qPrintable(string);
}

std::ostream& QInstaller::operator<<( std::ostream& os, const QByteArray& array )
{
    return os << '"' << QString::fromAscii( array ) << '"';
}

//TODO from kdupdaterfiledownloader.cpp, use that one once merged
QByteArray QInstaller::calculateHash( QIODevice* device, QCryptographicHash::Algorithm algo ) {
    Q_ASSERT( device );
    QCryptographicHash hash( algo );
    QByteArray buffer;
    buffer.resize( 512 * 1024 );
    while ( true ) {
        const qint64 numRead = device->read( buffer.data(), buffer.size() );
        if ( numRead <= 0 )
            return hash.result();
        hash.addData( buffer.constData(), numRead );
    }
    return QByteArray(); // never reached
}


QString QInstaller::replaceVariables(const QHash<QString,QString>& vars, const QString &str)
{
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf( QLatin1Char( '@' ), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf( QLatin1Char( '@' ), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += vars.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QString QInstaller::replaceWindowsEnvironmentVariables(const QString &str)
{
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString res;
    int pos = 0;
    while (true) {
        int pos1 = str.indexOf( QLatin1Char( '%' ), pos);
        if (pos1 == -1)
            break;
        int pos2 = str.indexOf( QLatin1Char( '%' ), pos1 + 1);
        if (pos2 == -1)
            break;
        res += str.mid(pos, pos1 - pos);
        QString name = str.mid(pos1 + 1, pos2 - pos1 - 1);
        res += env.value(name);
        pos = pos2 + 1;
    }
    res += str.mid(pos);
    return res;
}

QInstaller::VerboseWriter::VerboseWriter(QObject *parent) : QObject(parent)
{
    preFileBuffer.open(QIODevice::ReadWrite);
    stream.setDevice(&preFileBuffer);
}

QInstaller::VerboseWriter::~VerboseWriter()
{
    stream.flush();
    if (logFileName.isEmpty()) // binarycreator
        return;
    //if the installer installed nothing - there is no target directory - where the logfile can be saved
    if (!QFileInfo(logFileName).absoluteDir().exists()) {
        return;
    }
    QFile output(logFileName);
    if (!output.open(QIODevice::ReadWrite | QIODevice::Append)) {
        qWarning("Could not open logfile");
        return;
    }

    QString logInfo;
    logInfo += QLatin1String("*************************************");
    logInfo += QString::fromLatin1("Invoked:") + QDateTime::currentDateTime().toString();
    output.write(logInfo.toLocal8Bit());
    output.write(preFileBuffer.data());
    output.close();
}

void QInstaller::VerboseWriter::setOutputStream(const QString &fileName)
{
    logFileName = fileName;
}


Q_GLOBAL_STATIC(QInstaller::VerboseWriter, verboseWriter);

QInstaller::VerboseWriter* QInstaller::VerboseWriter::instance()
{
    return verboseWriter();
}

QInstaller::VerboseWriter& QInstaller::verbose()
{
    return *verboseWriter();
}
