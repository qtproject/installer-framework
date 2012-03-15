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

#ifndef KDTOOLS_KDSELFRESTARTER_H
#define KDTOOLS_KDSELFRESTARTER_H

#include "kdtoolsglobal.h"

class KDTOOLS_EXPORT KDSelfRestarter
{
public:
    KDSelfRestarter(int argc, char *argv[]);
    ~KDSelfRestarter();

    static bool restartOnQuit();
    static void setRestartOnQuit(bool restart);
    
private:
    Q_DISABLE_COPY(KDSelfRestarter)
    class Private;
    Private *d;
};

#endif // KDTOOLS_KDSELFRESTARTER_H
