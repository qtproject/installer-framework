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

#ifndef KD_UPDATER_UPDATE_OPERATIONS_H
#define KD_UPDATER_UPDATE_OPERATIONS_H

#include "kdupdaterupdateoperation.h"

namespace KDUpdater {

class KDTOOLS_EXPORT CopyOperation : public UpdateOperation
{
public:
    CopyOperation();
    ~CopyOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    CopyOperation *clone() const;

    QDomDocument toXml() const;
};

class KDTOOLS_EXPORT MoveOperation : public UpdateOperation
{
public:
    MoveOperation();
    ~MoveOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    MoveOperation *clone() const;
};

class KDTOOLS_EXPORT DeleteOperation : public UpdateOperation
{
public:
    DeleteOperation();
    ~DeleteOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    DeleteOperation *clone() const;

    QDomDocument toXml() const;
};

class KDTOOLS_EXPORT MkdirOperation : public UpdateOperation
{
public:
    MkdirOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    MkdirOperation *clone() const;
};

class KDTOOLS_EXPORT RmdirOperation : public UpdateOperation
{
public:
    RmdirOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    RmdirOperation *clone() const;
};

class KDTOOLS_EXPORT AppendFileOperation : public UpdateOperation
{
public:
    AppendFileOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    AppendFileOperation *clone() const;
};

class KDTOOLS_EXPORT PrependFileOperation : public UpdateOperation
{
public:
    PrependFileOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    PrependFileOperation *clone() const;
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_OPERATIONS_H
