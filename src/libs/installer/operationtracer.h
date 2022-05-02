/**************************************************************************
**
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
**************************************************************************/

#ifndef OPERATIONTRACER_H
#define OPERATIONTRACER_H

#include "qinstallerglobal.h"
#include "globals.h"

namespace QInstaller {

class INSTALLER_EXPORT AbstractOperationTracer
{
public:
    explicit AbstractOperationTracer(Operation *operation);
    virtual ~AbstractOperationTracer() = default;

    virtual void trace(const QString &state) = 0;

protected:
    Operation *m_operation;
};

class INSTALLER_EXPORT OperationTracer : public AbstractOperationTracer
{
public:
    explicit OperationTracer(Operation *operation);
    ~OperationTracer() override;

    void trace(const QString &state) override;
};

class INSTALLER_EXPORT ConcurrentOperationTracer : public AbstractOperationTracer
{
public:
    explicit ConcurrentOperationTracer(Operation *operation);

    void trace(const QString &state) override;
};

} // namespace QInstaller

#endif // OPERATIONTRACER_H
