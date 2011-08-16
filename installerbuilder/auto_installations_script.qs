var installerTargetDirectory="c:\\auto-test-installation";

function Controller()
{
    installer.autoRejectMessageBoxes;
    installer.setMessageBoxAutomaticAnswer( "OverwriteTargetDirectory", QMessageBox.Yes);
    //maybe we want something like this
    //installer.execute("D:\\cleanup_directory.bat", new Array(installerTargetDirectory));
    installer.setMessageBoxAutomaticAnswer( "stopProcessesForUpdates", QMessageBox.Ignore);
}


Controller.prototype.IntroductionPageCallback = function()
{
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("TargetDirectoryPage");
    page.TargetDirectoryLineEdit.setText(installerTargetDirectory);
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("ComponentSelectionPage");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.LicenseAgreementPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("LicenseAgreementPage");
    page.AcceptLicenseRadioButton.setChecked( true);
    gui.clickButton(buttons.NextButton);
}

////in the current installer we don't have this
//Controller.prototype.DynamicQtGuiPageCallback = function()
//{
//    var page = gui.pageWidgetByObjectName("DynamicQtGuiPage");
//    page.checkBoxLib.setChecked( false);
//    gui.clickButton(buttons.NextButton);
//}

////in the current installer we don't have this
//Controller.prototype.DynamicErrorPageCallback = function()
//{
//    var page = gui.pageWidgetByObjectName("DynamicErrorPage");
//    page.checkBoxMakeSure.setChecked( true);
//    gui.clickButton(buttons.NextButton);
//}

Controller.prototype.StartMenuDirectoryPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("StartMenuDirectoryPage");
    //page.LineEdit.text = "test";
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    gui.clickButton(buttons.NextButton);
}


Controller.prototype.PerformInstallationPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("PerformInstallationPage");
    gui.clickButton(buttons.NextButton);
}

Controller.prototype.FinishedPageCallback = function()
{
    var page = gui.pageWidgetByObjectName("FinishedPage");
    gui.clickButton(buttons.FinishButton);
}
