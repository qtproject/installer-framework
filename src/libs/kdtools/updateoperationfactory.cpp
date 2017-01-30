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

#include "updateoperationfactory.h"

#include "packagemanagercore.h"
#include "updateoperations.h"

using namespace KDUpdater;

/*!
    \inmodule kdupdater
    \class KDUpdater::UpdateOperationFactory
    \brief The UpdateOperationFactory class is used to create update operations based on their name.

    This class acts as a factory for \c KDUpdater::UpdateOperation. You can register one or more
    update operations with this factory and query operations based on their name.

    This class follows the singleton design pattern. Only one instance of this class can be created
    and its reference can be fetched from the instance() method.

    The following operations are registered by default:
    \list
        \li Copy operation
        \li Move operation
        \li Delete operation
        \li Mkdir operation
        \li Rmdir operation
        \li AppendFile operation
        \li PrependFile operation
    \endlist
*/

/*!
    \obsolete
    \fn void KDUpdater::UpdateOperationFactory::registerUpdateOperation(const QString &name)

    Registers a new update operation with the factory based on \a name. When create() is called
    with that \a name, the update operation is constructed using its default constructor.

    Deprecated. Use registerProduct() instead.
*/

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
}
