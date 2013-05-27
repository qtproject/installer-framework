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

#ifndef KD_UPDATER_UPDATE_INFO_DATA_H
#define KD_UPDATER_UPDATE_INFO_DATA_H

#include <QCoreApplication>
#include <QDomElement>
#include <QSharedData>

namespace KDUpdater {

struct UpdateInfo;

struct UpdatesInfoData : public QSharedData
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::UpdatesInfoData)

public:
    UpdatesInfoData();
    ~UpdatesInfoData();

    int error;
    QString errorMessage;
    QString updateXmlFile;
    QString applicationName;
    QString applicationVersion;
    QList<UpdateInfo> updateInfoList;

    void parseFile(const QString &updateXmlFile);
    bool parsePackageUpdateElement(const QDomElement &updateE);

    void setInvalidContentError(const QString &detail);
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_INFO_DATA_H
