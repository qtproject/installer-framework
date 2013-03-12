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
