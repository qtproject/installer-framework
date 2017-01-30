/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the FOO module of the Qt Toolkit.
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

function Component()
{
    component.loaded.connect(this, addRegisterFileCheckBox);
    installer.installationFinished.connect(this, addOpenFileCheckBoxToFinishPage);
    installer.finishButtonClicked.connect(this, openRegisteredFileIfChecked);
    component.unusualFileType = generateUnusualFileType(5)
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
addRegisterFileCheckBox = function()
{
    // don't show when updating or uninstalling
    if (installer.isInstaller()) {
        installer.addWizardPageItem(component, "RegisterFileCheckBoxForm", QInstaller.TargetDirectory);
        component.userInterface("RegisterFileCheckBoxForm").RegisterFileCheckBox.text =
            component.userInterface("RegisterFileCheckBoxForm").RegisterFileCheckBox.text + component.unusualFileType;
    }
}

// here we are creating the operation chain which will be processed at the real installation part later
Component.prototype.createOperations = function()
{
    // call default implementation to actually install the registeredfile
    component.createOperations();

    var isRegisterFileChecked = component.userInterface("RegisterFileCheckBoxForm").RegisterFileCheckBox.checked;
    if (installer.value("os") === "win") {
        var iconId = 0;
        var notepadPath = installer.environmentVariable("SystemRoot") + "\\notepad.exe";
        component.addOperation("RegisterFileType",
                               component.unusualFileType,
                               notepadPath + " '%1'",
                               "QInstaller Framework example file type",
                               "text/plain",
                               notepadPath + "," + iconId,
                               "ProgId=QtProject.QtInstallerFramework." + component.unusualFileType);
    }
    component.fileWithRegisteredType = installer.value("TargetDir") + "/registeredfile." + component.unusualFileType
    component.addOperation("Move", "@TargetDir@/registeredfile", component.fileWithRegisteredType);
}

openRegisteredFileIfChecked = function()
{
    if (!component.installed)
        return;

    if (installer.value("os") == "win" && installer.isInstaller() && installer.status == QInstaller.Success) {
        var isOpenRegisteredFileChecked = component.userInterface("OpenFileCheckBoxForm").OpenRegisteredFileCheckBox.checked;
        if (isOpenRegisteredFileChecked) {
            QDesktopServices.openUrl("file:///" + component.fileWithRegisteredType);
        }
    }
}

addOpenFileCheckBoxToFinishPage = function()
{
    if (installer.isInstaller() && installer.status == QInstaller.Success) {
        installer.addWizardPageItem(component, "OpenFileCheckBoxForm", QInstaller.InstallationFinished);
        component.userInterface("OpenFileCheckBoxForm").OpenRegisteredFileCheckBox.text =
            component.userInterface("OpenFileCheckBoxForm").OpenRegisteredFileCheckBox.text + component.unusualFileType;
    }
}
