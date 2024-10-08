/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#ifndef BINARYFORMATENGINEHANDLER_H
#define BINARYFORMATENGINEHANDLER_H

#include "binaryformat.h"

#include <QtCore/private/qabstractfileengine_p.h>

namespace QInstaller {

class INSTALLER_EXPORT BinaryFormatEngineHandler : public QAbstractFileEngineHandler
{
    Q_DISABLE_COPY(BinaryFormatEngineHandler)

public:
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    std::unique_ptr<QAbstractFileEngine> create(const QString &fileName) const;
#else
    QAbstractFileEngine *create(const QString &fileName) const;
#endif
    void clear();
    static BinaryFormatEngineHandler *instance();

    void registerResources(const QList<ResourceCollection> &collections);
    void registerResource(const QString &fileName, const QString &resourcePath);

private:
    BinaryFormatEngineHandler() {}
    ~BinaryFormatEngineHandler() {}

private:
    QHash<QByteArray, ResourceCollection> m_resources;
};

} // namespace QInstaller

#endif // BINARYFORMATENGINEHANDLER_H
