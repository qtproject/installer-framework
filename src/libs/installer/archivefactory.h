/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#ifndef ARCHIVEFACTORY_H
#define ARCHIVEFACTORY_H

#include "installer_global.h"
#include "genericfactory.h"
#include "abstractarchive.h"

namespace QInstaller {

class INSTALLER_EXPORT ArchiveFactory
    : public GenericFactory<AbstractArchive, QString, QString, QObject *>
{
    Q_DISABLE_COPY(ArchiveFactory)

public:
    static ArchiveFactory &instance();

    template <typename T>
    void registerArchive(const QString &name, const QStringList &types)
    {
        if (containsProduct(name))
            m_supportedTypesHash.remove(name);

        registerProduct<T>(name);
        m_supportedTypesHash.insert(name, types);
    }
    AbstractArchive *create(const QString &filename, QObject *parent = nullptr) const;

    static QStringList supportedTypes();
    static bool isSupportedType(const QString &filename);

private:
    ArchiveFactory();

private:
    QHash<QString, QStringList> m_supportedTypesHash;
};

} // namespace QInstaller

#endif // ARCHIVEFACTORY_H
