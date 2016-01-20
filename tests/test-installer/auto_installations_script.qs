/**************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
**************************************************************************/

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
