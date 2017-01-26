/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#ifndef KD_UPDATER_APPLICATION_H
#define KD_UPDATER_APPLICATION_H

#include "kdtoolsglobal.h"

#include <QSettings>

namespace KDUpdater {

class PackagesInfo;
class UpdateSourcesInfo;

class ConfigurationInterface
{
public:
    virtual ~ConfigurationInterface() {}
    virtual QVariant value(const QString &key) const
    {
        QSettings settings;
        settings.beginGroup(QLatin1String("KDUpdater"));
        return settings.value(key);
    }

    virtual void setValue(const QString &key, const QVariant &value)
    {
        QSettings settings;
        settings.beginGroup(QLatin1String("KDUpdater"));
        settings.setValue(key, value);
    }
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
