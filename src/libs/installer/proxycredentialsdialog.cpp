/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
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
**************************************************************************/
#include "proxycredentialsdialog.h"
#include "ui_proxycredentialsdialog.h"

#include <QNetworkProxy>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \namespace QInstaller::Ui
    \brief Groups user interface forms generated with Qt Designer.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::ProxyCredentialsDialog
    \internal
*/

ProxyCredentialsDialog::ProxyCredentialsDialog(const QNetworkProxy &proxy, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ProxyCredentialsDialog)
{
    setWindowTitle(tr("Proxy Credentials"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    ui->setupUi(this);

    setUserName(proxy.user());
    setPassword(proxy.password());

    const QString proxyString = QString::fromLatin1("%1:%2").arg(proxy.hostName()).arg(proxy.port());
    ui->infotext->setText(ui->infotext->text().arg(proxyString));
}

ProxyCredentialsDialog::~ProxyCredentialsDialog()
{
    delete ui;
}

QString ProxyCredentialsDialog::userName() const
{
    return ui->usernameLineEdit->text();
}

void ProxyCredentialsDialog::setUserName(const QString &username)
{
    ui->usernameLineEdit->setText(username);
}

QString ProxyCredentialsDialog::password() const
{
    return ui->passwordLineEdit->text();
}

void ProxyCredentialsDialog::setPassword(const QString &passwd)
{
    ui->passwordLineEdit->setText(passwd);
}

} // namespace QInstaller
