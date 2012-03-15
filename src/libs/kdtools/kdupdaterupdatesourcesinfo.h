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

#ifndef KD_UPDATER_UPDATE_SOURCES_INFO_H
#define KD_UPDATER_UPDATE_SOURCES_INFO_H

#include "kdupdater.h"

#include <QObject>
#include <QVariant>
#include <QUrl>

namespace KDUpdater {

class Application;

struct KDTOOLS_EXPORT UpdateSourceInfo
{
    UpdateSourceInfo() : priority(-1) { }

    QString name;
    QString title;
    QString description;
    QUrl url;
    int priority;
};

KDTOOLS_EXPORT bool operator==(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs);

inline bool operator!=(const UpdateSourceInfo &lhs, const UpdateSourceInfo &rhs)
{
    return !operator==(lhs, rhs);
}

class KDTOOLS_EXPORT UpdateSourcesInfo : public QObject
{
    Q_OBJECT

public:
    ~UpdateSourcesInfo();

    enum Error
    {
        NoError = 0,
        NotYetReadError,
        CouldNotReadSourceFileError,
        InvalidXmlError,
        InvalidContentError,
        CouldNotSaveChangesError
    };

    Application *application() const;

    bool isValid() const;
    QString errorString() const;
    Error error() const;

    bool isModified() const;
    void setModified(bool modified);

    void setFileName(const QString &fileName);
    QString fileName() const;

    int updateSourceInfoCount() const;
    UpdateSourceInfo updateSourceInfo(int index) const;

    void addUpdateSourceInfo(const UpdateSourceInfo &info);
    void removeUpdateSourceInfo(const UpdateSourceInfo &info);
    void removeUpdateSourceInfoAt(int index);
    void setUpdateSourceInfoAt(int index, const UpdateSourceInfo &info);

protected:
    explicit UpdateSourcesInfo(Application *application);

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void reset();
    void updateSourceInfoAdded(const UpdateSourceInfo &info);
    void updateSourceInfoRemoved(const UpdateSourceInfo &info);
    void updateSourceInfoChanged(const UpdateSourceInfo &newInfo,
                                 const UpdateSourceInfo &oldInfo);

private:
    friend class Application;
    struct UpdateSourcesInfoData;
    UpdateSourcesInfoData *d;
};

} // namespace KDUpdater

Q_DECLARE_METATYPE(KDUpdater::UpdateSourceInfo)

#endif // KD_UPDATER_UPDATE_SOURCES_INFO_H
