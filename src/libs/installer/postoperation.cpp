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
