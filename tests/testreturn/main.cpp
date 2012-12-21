/**************************************************************************
**
** Copyright (C) 2012 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
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
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/
#include <QFile>
#include <QString>
#include <iostream>

static void crash() {
    QString* nemesis = 0;
    nemesis->clear();
}

int main( int argc, char** argv ) {
    std::cout << "Hello." << std::endl;
    if ( argc < 2 )
        return 0;
    const QString arg = QString::fromLocal8Bit( argv[1] ); 
    bool ok = false;
    const int num = arg.toInt( &ok );
    if ( ok )
        return num;
    if ( arg == QLatin1String("crash") ) {
        std::cout << "Yeth, mather. I Crash." << std::endl;
        crash();
    }
    if ( arg == QLatin1String("--script") ) {
        const QString fn = QString::fromLocal8Bit( argv[2] ); 
        QFile f( fn );
        if ( !f.open( QIODevice::ReadOnly ) ) {
            std::cerr << "Could not open file for reading: " << qPrintable(f.errorString()) << std::endl;
            crash();
        }
        bool ok;
        const int rv = QString::fromLatin1( f.readAll() ).toInt( &ok );
        if ( ok )
            return rv;
        else
            crash();
    }

}
