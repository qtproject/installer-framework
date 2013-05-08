function Controller()
{
    installer.setMessageBoxAutomaticAnswer("OverwriteTargetDirectory", QMessageBox.Yes);
    installer.setValue("GuiTestValue", "hello");
}

Controller.prototype.ComponentSelectionPageCallback = function()
{
    var notExistingButtonId = 9999999;
    gui.clickButton(notExistingButtonId);
}

Controller.prototype.FinishedPageCallback = function()
{
    print("FinishedPageCallback - OK")
}
