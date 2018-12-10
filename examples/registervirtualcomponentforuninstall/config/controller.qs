function Controller() {

}

Controller.prototype.ComponentSelectionPageCallback = function() {

    var page = gui.pageWidgetByObjectName("ComponentSelectionPage");
    page.addVirtualComponentToUninstall("component")
}
