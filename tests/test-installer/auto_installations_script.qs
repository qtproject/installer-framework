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
