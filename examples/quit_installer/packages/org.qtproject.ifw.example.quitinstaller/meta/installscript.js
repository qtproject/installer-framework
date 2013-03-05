function Component()
{
    var result = QMessageBox["question"]("test.quit", "Installer", "Do you want to quit the installer?<br>" +
        "This message box was created through javascript.", QMessageBox.Yes | QMessageBox.No);
    if (result == QMessageBox.Yes) {
        installer.setValue("FinishedText", "<font color='red' size=10>This installer was aborted.</font>");
        installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
        installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
        installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
        installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
        installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
    } else {
        installer.setValue("FinishedText",
            "<font color='green' size=10>The installer was not quit from javascript.</font>");
    }
}
