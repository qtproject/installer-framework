/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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

#include "postoperation.h"

#include "packagemanagercore.h"
#include "globals.h"

#include <QtCore/QDebug>
#include <QEventLoop>

using namespace QInstaller;

PostOperation::PostOperation(PackageManagerCore *core)
    : UpdateOperation(core)
{
    setName(QLatin1String("Post"));
}

void PostOperation::backup()
{
}

bool PostOperation::performOperation()
{
    if (!checkArgumentCount(2, 2, tr(" (Url, Payload)")))
        return false;

    const QStringList args = arguments();
    const QString url = args.at(0);
    const QString payloadData = args.at(1);

    QNetworkAccessManager* nam = new QNetworkAccessManager();
    QByteArray payload(payloadData.toStdString().c_str());
    QEventLoop synchronous;
    QNetworkRequest request;
    
    connect(nam, SIGNAL(finished(QNetworkReply*)), &synchronous, SLOT(quit()));
    request.setUrl(QUrl(url));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QLatin1String("application/x-www-form-urlencoded"));

    QNetworkReply* reply = nam->post(request, payload);
    
    connect(reply, SIGNAL(finished()), this, SLOT(onFinished()));
    connect(reply, SIGNAL(error(QNetworkReply::NetworkError)), this, SLOT(onError(QNetworkReply::NetworkError)));
    
    synchronous.exec();
	
    return true;
}

bool PostOperation::undoOperation()
{
    return true;
}

bool PostOperation::testOperation()
{
    return true;
}

void PostOperation::onError(QNetworkReply::NetworkError code)
{
    qDebug() << "Failed to send HTTP POST, error code: " << code;
}

void PostOperation::onFinished()
{
    qDebug() << "HTTP POST sent";
}
