/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
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
**
** $QT_END_LICENSE$
**
**************************************************************************/

function Component()
{
    var result = QMessageBox["question"]("test.quit", "Installer", "Do you want to quit the installer?<br>" +
        "This message box was created using JavaScript.", QMessageBox.Ok | QMessageBox.Cancel);
    if (result == QMessageBox.Ok) {
        installer.setValue("FinishedText", "<font color='red' size=3>The installer was quit.</font>");
        installer.setDefaultPageVisible(QInstaller.TargetDirectory, false);
        installer.setDefaultPageVisible(QInstaller.ReadyForInstallation, false);
        installer.setDefaultPageVisible(QInstaller.ComponentSelection, false);
        installer.setDefaultPageVisible(QInstaller.StartMenuSelection, false);
        installer.setDefaultPageVisible(QInstaller.PerformInstallation, false);
        installer.setDefaultPageVisible(QInstaller.LicenseCheck, false);
        gui.clickButton(buttons.NextButton);
    } else {
        installer.setValue("FinishedText",
            "<font color='green' size=3>The installer was not quit by JavaScript.</font>");
    }
}
