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

#ifndef BINARYLAYOUT_H
#define BINARYLAYOUT_H

#include "range.h"

#include <QVector>

namespace QInstaller {

struct BinaryLayout
{
    qint64 endOfExectuable;

    QVector<Range<qint64> > metaResourceSegments;

    Range<qint64> metaResourcesSegment;
    Range<qint64> operationsSegment;
    Range<qint64> resourceCollectionsSegment;

    qint64 binaryContentSize;
    qint64 magicMarker;
    quint64 magicCookie;

    qint64 endOfBinaryContent;
};

} // namespace QInstaller

#endif // BINARYLAYOUT
