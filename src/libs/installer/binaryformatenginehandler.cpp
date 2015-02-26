/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
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
**************************************************************************/

#include "binaryformatengine.h"
#include "binaryformatenginehandler.h"
#include "productkeycheck.h"

namespace QInstaller {

/*!
    \class QInstaller::BinaryFormatEngineHandler
    \inmodule QtInstallerFramework
    \brief The BinaryFormatEngineHandler class provides a way to register resource collections and
        resource files.
*/

/*!
    Creates a file engine for the file specified by \a fileName. To be able to create a file
    engine, the file name needs to be prefixed with \c {installer://}.

    Returns 0 if the engine cannot handle \a fileName.
*/
QAbstractFileEngine *BinaryFormatEngineHandler::create(const QString &fileName) const
{
    return fileName.startsWith(QLatin1String("installer://"), Qt::CaseInsensitive )
        ? new BinaryFormatEngine(m_resources, fileName) : 0;
}

/*!
    Clears the contents of the binary format engine.
*/
void BinaryFormatEngineHandler::clear()
{
    m_resources.clear();
}

/*!
    Returns the active instance of the engine.
*/
BinaryFormatEngineHandler *BinaryFormatEngineHandler::instance()
{
    static BinaryFormatEngineHandler instance;
    return &instance;
}

/*!
    Registers the given resource collections \a collections in the engine.
*/
void BinaryFormatEngineHandler::registerResources(const QList<ResourceCollection> &collections)
{
    foreach (const ResourceCollection &collection, collections) {
        if (ProductKeyCheck::instance()->isValidPackage(QString::fromUtf8(collection.name())))
            m_resources.insert(collection.name(), collection);
    }
}

/*!
    Registers the resource specified by \a resourcePath in a resource collection specified
    by \a fileName. The file name \a fileName must be in the form of \c {installer://}, followed
    by the collection name and resource name separated by a forward slash.

    A valid file name looks like this: installer://collectionName/resourceName
*/
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

    if (!ProductKeyCheck::instance()->isValidPackage(QString::fromUtf8(collectionName)))
        return;

    m_resources[collectionName].setName(collectionName);
    m_resources[collectionName].appendResource(QSharedPointer<Resource>(new Resource(resourcePath,
        resourceName)));
}

} // namespace QInstaller
