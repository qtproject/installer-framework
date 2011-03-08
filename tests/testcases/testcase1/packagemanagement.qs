function Controller()
{
    installer.autoRejectMessageBoxes
    this.componentSelectionCounter = 0
}

Controller.prototype.UpdaterSelectedCallback = function()
{
    tabController.setCurrentTab( TabController.PACKAGE_MANAGER )
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    if ( this.componentSelectionCounter == 0 ) {
        print( "first time, uninstall" )
        var page = gui.pageWidgetByObjectName( "ComponentSelectionPage" )
        page.deselectComponent( "com.nokia.sdk.doc.qtcreator" )
        gui.clickButton( buttons.NextButton, 3000 )
        this.componentSelectionCounter += 1
    } else {
        print( "second time, click cancel" )
        gui.clickButton( buttons.CancelButton )
    }
}


Controller.prototype.ReadyForInstallationPageCallback = function()
{
    gui.clickButton( buttons.NextButton )
}

Controller.prototype.FinishedPageCallback = function()
{
    gui.clickButton( buttons.CommitButton )
}
