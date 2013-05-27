/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#ifndef KD_UPDATER_UPDATE_INFO_H
#define KD_UPDATER_UPDATE_INFO_H

#include "kdupdater.h"
#include "kdupdaterupdatesinfodata_p.h"

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

#endif // KD_UPDATER_UPDATE_INFO_H
