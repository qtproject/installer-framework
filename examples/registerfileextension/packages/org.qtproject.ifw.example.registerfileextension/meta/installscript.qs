/**************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

function Component()
{
    component.loaded.connect(this, my_componentLoaded);
    installer.finishButtonClicked.connect(this, my_installationFinished);
    installer.installationFinished.connect(this, my_installationFinishedPageIsShown);
    component.unusalFileType = generateUnusualFileType(5)
}

generateUnusualFileType = function(length)
{
    var randomString = "";
    var possible = "abcdefghijklmnopqrstuvwxyz0123456789";

    for (var i = 0; i < length; i++)
        randomString += possible.charAt(Math.floor(Math.random() * possible.length));
    return randomString;
}

// called as soon as the component was loaded
my_componentLoaded = function()
{
    // don't show when updating / de-installing
    if (installer.isInstaller()) {
        installer.addWizardPageItem(component, "RegisterFileCheckBoxesForm", QInstaller.TargetDirectory);
        component.userInterface("RegisterFileCheckBoxesForm").RegisterFileCheckBox.text =
            component.userInterface("RegisterFileCheckBoxesForm").RegisterFileCheckBox.text + component.unusalFileType;
    }
}

// called after everything is set up, but before any fie is written
Component.prototype.beginInstallation = function()
{
    // call default implementation which is necessary for most hooks
    // in beginInstallation case it makes nothing
    component.beginInstallation();

    component.registeredFile = installer.value("TargetDir") + "/registeredfile." + component.unusalFileType;
}

// here we are creating the operation chain which will be processed at the real installation part later
Component.prototype.createOperations = function()
{
    // call default implementation to actually install the registeredfile
    component.createOperations();
    var iconId = 0;
    var notepadPath = installer.environmentVariable("SystemRoot") + "\\notepad.exe";

    var isRegisterFileChecked = component.userInterface("RegisterFileCheckBoxesForm").RegisterFileCheckBox.checked;
    if (installer.value("os") === "win") {
        component.addOperation("RegisterFileType",
                               component.unusalFileType,
                               notepadPath + " '%1'",
                               "QInstaller Framework example file type",
                               "text/plain",
                               notepadPath + "," + iconId,
                               "ProgId=QtProject.QtInstallerFramework." + component.unusalFileType);
    }
    component.addOperation("Move", "@TargetDir@/registeredfile", component.registeredFile);
}

my_installationFinished = function()
{
    if (!component.installed)
        return;

    if (installer.value("os") == "win" && installer.isInstaller() && installer.status == QInstaller.Success) {
        var isOpenRegisteredFileChecked = component.userInterface("OpenFileCheckBoxesForm").OpenRegisteredFileCheckBox.checked;
        if (isOpenRegisteredFileChecked) {
            QDesktopServices.openUrl("file:///" + component.registeredFile);
        }
    }
}

my_installationFinishedPageIsShown = function()
{
    if (installer.isInstaller() && installer.status == QInstaller.Success) {
        installer.addWizardPageItem(component, "OpenFileCheckBoxesForm", QInstaller.InstallationFinished);
        component.userInterface("OpenFileCheckBoxesForm").OpenRegisteredFileCheckBox.text =
            component.userInterface("OpenFileCheckBoxesForm").OpenRegisteredFileCheckBox.text + component.unusalFileType;
    }
}
