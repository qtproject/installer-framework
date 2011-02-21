function Controller()
{
    installer.autoRejectMessageBoxes
    installer.setMessageBoxAutomaticAnswer( "overwriteTargetDirectory", QMessageBox.Yes )
}


Controller.prototype.IntroductionPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.LicenseAgreementPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "LicenseAgreementPage" )
    page.acceptLicenseRB.setChecked( true )
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.TargetDirectoryPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "TargetDirectoryPage" )
    page.targetDirectoryLE.setText( "c:\\Users\\kdab\\Desktop\\testinstall" )
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "ComponentSelectionPage" )
    page.deselectComponent( "com.nokia.sdk.qtcreator" )
    page.deselectComponent( "hgrmpfl (non-existing package)" ) // bad case for component lookup
    page.selectComponent( "hgrmpfl2 (another non-existing package)" ) // bad case for component lookup
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.DynamicQtGuiPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "DynamicQtGuiPage" )
    page.checkBoxLib.setChecked( false )
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.DynamicErrorPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "DynamicErrorPage" )
    page.checkBoxMakeSure.setChecked( true )
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.StartMenuDirectoryPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.PerformInstallationPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "PerformInstallationPage" )
    page.details.button.click
}

Controller.prototype.FinishedPageCallback = function()
{
    gui.clickButton( buttons.FinishButton )
}
