function Controller()
{
    installer.autoRejectMessageBoxes
}


Controller.prototype.IntroductionPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    var page = gui.pageWidgetByObjectName( "ComponentSelectionPage" )
    page.keepSelectedComponentsRB.setChecked( true )
    page.deselectComponent( "com.nokia.sdk.qt.gui" )
    page.deselectComponent( "hgrmpfl (non-existing package)" ) // bad case for component lookup
    page.selectComponent( "hgrmpfl2 (another non-existing package)" ) // bad case for component lookup
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.FinishedPageCallback = function()
{
    gui.clickButton( buttons.FinishButton )
}
