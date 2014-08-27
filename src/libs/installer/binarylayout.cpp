/**************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
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

/*!
    \class QInstaller::BinaryLayout

    BinaryLayout handles binary information embedded into executables.
    Qt resources as well as resource collections can be stored.

    Explanation of the binary blob at the end of the installer or separate data file:

    \code

    Meta data entry [1 ... n]
    [Format]
        Plain data (QResource)
    [Format]
    ----------------------------------------------------------
    Operation count (qint64)
    Operation entry [1 ... n]
    [Format]
        Name (qint64, QString)
        XML (qint64, QString)
    [Format]
    Operation count (qint64)
    ----------------------------------------------------------
    Component count
    Component data entry [1 ... n]
    [Format]
        Archive count (qint64),
        Name entry [1 ... n]
        [Format]
            Name (qint64, QByteArray),
            Offset (qint64),
            Length (qint64),
        [Format]
        Archive data entry [1 ... n]
        [Format]
            Plain data
        [Format]
    [Format]
    ----------------------------------------------------------
    Component count (qint64)
    Component index entry [1 ... n]
    [Format]
        Name (qint64, QByteArray)
        Offset (qint64)
        Length (qint64)
    [Format]
    Component count (qint64)
    ----------------------------------------------------------
    Component index block [Offset (qint64)]
    Component index block [Length (qint64)]
    ----------------------------------------------------------
    Resource segments [1 ... n]
    [Format]
        Offset (qint64)
        Length (qint64)
    [Format]
    ----------------------------------------------------------
    Operations information block [Offset (qint64)]
    Operations information block [Length (qint64)]
    ----------------------------------------------------------
    Meta data count (qint64)
    ----------------------------------------------------------
    Binary content size [Including Marker and Cookie (qint64)]
    ----------------------------------------------------------
    Magic marker (qint64)
    Magic cookie (qint64)

    \endcode
*/
