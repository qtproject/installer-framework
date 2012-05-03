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

#include <QSharedDataPointer>
#include <QString>
#include <QDate>
#include <QList>
#include <QStringList>
#include <QUrl>
#include <QHash>
#include <QVariant>

// Classes and structures in this header file are for internal use only.
// They are not a part of the public API

namespace KDUpdater {

struct UpdateFileInfo
{
    UpdateFileInfo()
        : compressedSize(0),
          uncompressedSize(0)
    {}

    QString arch;
    QString os;
    QString fileName;
    QByteArray sha1sum;
    quint64 compressedSize;
    quint64 uncompressedSize;
};

struct UpdateInfo
{
    int type;
    QHash<QString, QVariant> data;
    QList<UpdateFileInfo> updateFiles;
};

class UpdatesInfo
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
    QString errorString() const;
    Error error() const;

    void setFileName(const QString &updateXmlFile);
    QString fileName() const;

    QString applicationName() const;
    QString applicationVersion() const;
    int compatLevel() const;

    int updateInfoCount(int type = AllUpdate) const;
    UpdateInfo updateInfo(int index) const;
    QList<UpdateInfo> updatesInfo(int type = AllUpdate, int compatLevel = -1) const;

private:
    struct UpdatesInfoData;
    QSharedDataPointer<UpdatesInfoData> d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_INFO_H
