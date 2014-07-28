/**************************************************************************
**
** Copyright (C) 2012-2014 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "binaryformatenginehandler.h"
#include "binaryformatengine.h"

#include <QFile>

namespace QInstaller {

QAbstractFileEngine *BinaryFormatEngineHandler::create(const QString &fileName) const
{
    return fileName.startsWith(QLatin1String("installer://"), Qt::CaseInsensitive )
        ? new BinaryFormatEngine(m_resources, fileName) : 0;
}

void BinaryFormatEngineHandler::reset()
{
    m_resources.clear();
}

BinaryFormatEngineHandler *BinaryFormatEngineHandler::instance()
{
    static BinaryFormatEngineHandler instance;
    return &instance;
}

void BinaryFormatEngineHandler::registerResources(const QList<ResourceCollection> &collections)
{
    foreach (const ResourceCollection &collection, collections)
        m_resources.insert(collection.name(), collection);
}

void
BinaryFormatEngineHandler::registerResource(const QString &fileName, const QString &resourcePath)
{
    static const QChar sep = QChar::fromLatin1('/');
    static const QString prefix = QString::fromLatin1("installer://");
    Q_ASSERT(fileName.toLower().startsWith(prefix));

    // cut the prefix
    QString path = fileName.mid(prefix.length());
    while (path.endsWith(sep))
        path.chop(1);

    const QByteArray resourceName = path.section(sep, 1, 1).toUtf8();
    const QByteArray collectionName = path.section(sep, 0, 0).toUtf8();
    m_resources[collectionName].appendResource(QSharedPointer<Resource>(new Resource(resourceName,
        resourcePath)));
}

} // namespace QInstaller
