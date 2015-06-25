/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef UPDATESINFO_P_H
#define UPDATESINFO_P_H

#include "updater.h"
#include "updatesinfodata_p.h"

#include <QHash>
#include <QSharedData>
#include <QVariant>

// They are not a part of the public API
// Classes and structures in this header file are for internal use only but still exported for auto tests.

namespace KDUpdater {

struct KDTOOLS_EXPORT UpdateInfo
{
    QHash<QString, QVariant> data;
};

class KDTOOLS_EXPORT UpdatesInfo
{
public:
    enum Error
    {
        NoError = 0,
        NotYetReadError,
        CouldNotReadUpdateInfoFileError,
        InvalidXmlError,
        InvalidContentError
    };

    UpdatesInfo();
    ~UpdatesInfo();

    bool isValid() const;

    Error error() const;
    QString errorString() const;

    QString fileName() const;
    void setFileName(const QString &updateXmlFile);

    QString applicationName() const;
    QString applicationVersion() const;

    int updateInfoCount() const;
    UpdateInfo updateInfo(int index) const;
    QList<UpdateInfo> updatesInfo() const;

private:
    QSharedDataPointer<UpdatesInfoData> d;
};

} // namespace KDUpdater

#endif // UPDATESINFO_P_H
