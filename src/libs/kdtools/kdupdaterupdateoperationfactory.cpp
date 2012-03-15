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

#include "kdupdaterupdateoperationfactory.h"
#include "kdupdaterupdateoperations.h"

#include <QHash>

/*!
   \ingroup kdupdater
   \class KDUpdater::UpdateOperationFactory kdupdaterupdateoperationfactory.h KDUpdaterUpdateOperationFactory
   \brief Factory for \ref KDUpdater::UpdateOperation

   This class acts as a factory for \ref KDUpdater::UpdateOperation. You can register
   one or more update operations with this factory and query operations based on their name.

   This class follows the singleton design pattern. Only one instance of this class can
   be created and its reference can be fetched from the \ref instance() method.
*/

/*!
   \fn KDUpdater::UpdateOperationFactory::registerUpdateOperation( const QString& name )

   Registers T as new UpdateOperation with \a name. When create() is called with that \a name,
   T is constructed using its default constructor.
*/

using namespace KDUpdater;

/*!
   Returns the UpdateOperationFactory instance. The instance is created if needed.
*/
UpdateOperationFactory &UpdateOperationFactory::instance()
{
    static UpdateOperationFactory theFactory;
    return theFactory;
}

/*!
   Constructor
*/
UpdateOperationFactory::UpdateOperationFactory()
{
    // Register the default update operation set
    registerUpdateOperation<CopyOperation>(QLatin1String("Copy"));
    registerUpdateOperation<MoveOperation>(QLatin1String("Move"));
    registerUpdateOperation<DeleteOperation>(QLatin1String("Delete"));
    registerUpdateOperation<MkdirOperation>(QLatin1String("Mkdir"));
    registerUpdateOperation<RmdirOperation>(QLatin1String("Rmdir"));
    registerUpdateOperation<AppendFileOperation>(QLatin1String("AppendFile"));
    registerUpdateOperation<PrependFileOperation>(QLatin1String("PrependFile"));
    registerUpdateOperation<ExecuteOperation>(QLatin1String("Execute"));
    registerUpdateOperation<UpdatePackageOperation>(QLatin1String("UpdatePackage"));
    registerUpdateOperation<UpdateCompatOperation>(QLatin1String("UpdateCompat"));
}
