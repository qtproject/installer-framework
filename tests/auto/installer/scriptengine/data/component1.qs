/**************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
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
** $QT_END_LICENSE$
**
**************************************************************************/

function Component()
{
    print("Component constructor - OK");
}

Component.prototype.retranslateUi = function()
{
    // arguments.callee to get the current function name doesn't work in that case
    print("retranslateUi - OK");
    // no default implementation for this method
    // component.languageChanged();
}

Component.prototype.createOperationsForPath = function(path)
{
    print("createOperationsForPath - OK");
    component.createOperationsForPath(path);
}

Component.prototype.createOperationsForArchive = function(archive)
{
    print("createOperationsForArchive - OK");
    component.createOperationsForArchive(archive);
}

Component.prototype.beginInstallation = function()
{
    print("beginInstallation - OK");
    component.beginInstallation();
}

Component.prototype.createOperations = function()
{
    print("createOperations - OK");
    component.createOperations();
}

Component.prototype.isDefault = function()
{
    print("isDefault - OK");
    return false;
}
