/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2012 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#include "binaryformatenginehandler.h"
#include "binaryformatengine.h"
#include "binaryformat.h"

#include <QDebug>
#include <QFile>
#include <QFSFileEngine>

using namespace QInstallerCreator;

static BinaryFormatEngineHandler *s_instance = 0;

    
class BinaryFormatEngineHandler::Private
{
public:
    Private(const ComponentIndex &i)
        : index(i)
    {
    }

    ComponentIndex index;
};

BinaryFormatEngineHandler::BinaryFormatEngineHandler(const ComponentIndex &index)
    : d(new Private(index))
{
    s_instance = this;
}

BinaryFormatEngineHandler::BinaryFormatEngineHandler(const BinaryFormatEngineHandler &other)
    : QAbstractFileEngineHandler(),
      d(new Private(other.d->index))
{
    s_instance = this;
}

BinaryFormatEngineHandler::~BinaryFormatEngineHandler()
{
    if (s_instance == this)
        s_instance = 0;
    delete d;
}

void BinaryFormatEngineHandler::setComponentIndex(const ComponentIndex &index)
{
    d->index = index;
}

QAbstractFileEngine *BinaryFormatEngineHandler::create(const QString &fileName) const
{
    return fileName.startsWith(QLatin1String("installer://"), Qt::CaseInsensitive ) ? new BinaryFormatEngine(d->index, fileName) : 0;
}
    
BinaryFormatEngineHandler *BinaryFormatEngineHandler::instance()
{
    return s_instance;
}
   
void BinaryFormatEngineHandler::registerArchive(const QString &pathName, const QString &archive)
{
    static const QChar sep = QChar::fromLatin1('/');
    static const QString prefix = QString::fromLatin1("installer://");
    Q_ASSERT(pathName.toLower().startsWith(prefix));

    // cut the prefix
    QString path = pathName.mid(prefix.length());
    while (path.endsWith(sep))
        path.chop(1);

    const QString comp = path.section(sep, 0, 0);
    const QString archiveName = path.section(sep, 1, 1);
    
    Component c = d->index.componentByName(comp.toUtf8());
    if (c.name().isEmpty())
        c.setName(comp.toUtf8());

    QList< QSharedPointer<Archive> > registered;
    QSharedPointer<Archive> newArchive(new Archive(archive));
    newArchive->setName(archiveName.toUtf8());
    registered.push_back(newArchive);
    c.appendArchive(newArchive);
    d->index.insertComponent(c);
}
