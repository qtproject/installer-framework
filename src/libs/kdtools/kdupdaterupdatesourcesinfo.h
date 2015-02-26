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

#ifndef KD_UPDATER_UPDATE_SOURCES_INFO_H
#define KD_UPDATER_UPDATE_SOURCES_INFO_H

#include "kdtoolsglobal.h"

#include <QObject>
#include <QVariant>
#include <QUrl>

namespace KDUpdater {

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

protected:
    friend class Application;
    explicit UpdateSourcesInfo(QObject *parent = 0);

public Q_SLOTS:
    void refresh();

Q_SIGNALS:
    void reset();
    void updateSourceInfoAdded(const UpdateSourceInfo &info);
    void updateSourceInfoRemoved(const UpdateSourceInfo &info);

private:
    struct UpdateSourcesInfoData;
    QScopedPointer<UpdateSourcesInfoData> d;
};

} // namespace KDUpdater

Q_DECLARE_METATYPE(KDUpdater::UpdateSourceInfo)

#endif // KD_UPDATER_UPDATE_SOURCES_INFO_H
