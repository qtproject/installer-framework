/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
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
