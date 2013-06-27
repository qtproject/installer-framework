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
****************************************************************************/

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
}
