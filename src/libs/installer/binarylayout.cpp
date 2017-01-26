/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

/*!
    \class QInstaller::BinaryLayout
    \inmodule QtInstallerFramework
    \brief The BinaryLayout class describes the binary content appended to a file.

    Explanation of the binary content at the end of the installer or the separate data file:

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
    Collection count
    Collection data entry [1 ... n]
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
    Collection count (qint64)
    Collection index entry [1 ... n]
    [Format]
        Name (qint64, QByteArray)
        Offset (qint64)
        Length (qint64)
    [Format]
    Collection count (qint64)
    ----------------------------------------------------------
    Collection index block [Offset (qint64)]
    Collection index block [Length (qint64)]
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
