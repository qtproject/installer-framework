/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2022 The Qt Company Ltd.
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

#ifndef UPDATEOPERATIONS_H
#define UPDATEOPERATIONS_H

#include "updateoperation.h"

namespace KDUpdater {

class KDTOOLS_EXPORT CopyOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::CopyOperation)
public:
    explicit CopyOperation(QInstaller::PackageManagerCore *core = 0);
    ~CopyOperation();

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;

    QDomDocument toXml() const override;
private:
    QString sourcePath();
    QString destinationPath();
};

class KDTOOLS_EXPORT MoveOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::MoveOperation)
public:
    explicit MoveOperation(QInstaller::PackageManagerCore *core = 0);
    ~MoveOperation();

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;
};

class KDTOOLS_EXPORT DeleteOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::DeleteOperation)
public:
    explicit DeleteOperation(QInstaller::PackageManagerCore *core = 0);
    ~DeleteOperation();

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;

    QDomDocument toXml() const override;
};

class KDTOOLS_EXPORT MkdirOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::MkdirOperation)
public:
    explicit MkdirOperation(QInstaller::PackageManagerCore *core = 0);

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;
};

class KDTOOLS_EXPORT RmdirOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::RmdirOperation)
public:
    RmdirOperation(QInstaller::PackageManagerCore *core = 0);

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;
};

class KDTOOLS_EXPORT AppendFileOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::AppendFileOperation)
public:
    explicit AppendFileOperation(QInstaller::PackageManagerCore *core = 0);
    ~AppendFileOperation();

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;
};

class KDTOOLS_EXPORT PrependFileOperation : public UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(KDUpdater::PrependFileOperation)
public:
    explicit PrependFileOperation(QInstaller::PackageManagerCore *core = 0);
    ~PrependFileOperation();

    void backup() override;
    bool performOperation() override;
    bool undoOperation() override;
    bool testOperation() override;
};

} // namespace KDUpdater

#endif // UPDATEOPERATIONS_H
