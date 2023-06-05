/**************************************************************************
**
** Copyright (C) 2023 The Qt Company Ltd.
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

#ifndef METADATA_H
#define METADATA_H

#include "installer_global.h"
#include "genericdatacache.h"
#include "repository.h"

#include <QDomDocument>

class QFile;

namespace QInstaller {

class INSTALLER_EXPORT Metadata : public CacheableItem
{
public:
    Metadata();
    explicit Metadata(const QString &path);
    ~Metadata() {}

    QByteArray checksum() const override;
    void setChecksum(const QByteArray &checksum);
    QDomDocument updatesDocument() const;

    bool isValid() const override;
    bool isActive() const override;
    bool obsoletes(CacheableItem *other) override;

    Repository repository() const;
    void setRepository(const Repository &repository);

    bool isAvailableFromDefaultRepository() const;
    void setAvailableFromDefaultRepository(bool defaultRepository);

    void setPersistentRepositoryPath(const QUrl &url);
    QString persistentRepositoryPath();

    bool containsRepositoryUpdates() const;

private:
    bool verifyMetaFiles(QFile *updateFile) const;

private:
    Repository m_repository;
    QString m_persistentRepositoryPath;
    mutable QByteArray m_checksum;

    bool m_fromDefaultRepository;
};

} // namespace QInstaller

#endif // METADATA_H
