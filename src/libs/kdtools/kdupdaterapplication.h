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

#ifndef KD_UPDATER_APPLICATION_H
#define KD_UPDATER_APPLICATION_H

#include "kdupdater.h"
#include <QObject>

QT_BEGIN_NAMESPACE
class QUrl;
QT_END_NAMESPACE

namespace KDUpdater {

class PackagesInfo;
class UpdateSourcesInfo;

class ConfigurationInterface
{
public:
    virtual ~ConfigurationInterface();
    virtual QVariant value(const QString &key ) const = 0;
    virtual void setValue(const QString &key, const QVariant &value) = 0;
};

class KDTOOLS_EXPORT Application : public QObject
{
    Q_OBJECT

public:
    explicit Application(ConfigurationInterface *config = 0, QObject *parent = 0);
    ~Application();

    static Application *instance();

    void setApplicationDirectory(const QString &dir);
    QString applicationDirectory() const;

    QString applicationName() const;
    QString applicationVersion() const;
    int compatLevel() const;

    void setPackagesXMLFileName(const QString &fileName);
    QString packagesXMLFileName() const;
    PackagesInfo *packagesInfo() const;

    void addUpdateSource(const QString &name, const QString &title,
                         const QString &description, const QUrl &url, int priority = -1);

    void setUpdateSourcesXMLFileName(const QString &fileName);
    QString updateSourcesXMLFileName() const;
    UpdateSourcesInfo *updateSourcesInfo() const;

    QStringList filesForDelayedDeletion() const;
    void addFilesForDelayedDeletion(const QStringList &files);

public Q_SLOTS:
    void printError(int errorCode, const QString &error);

private:
    struct ApplicationData;
    ApplicationData *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_APPLICATION_H
