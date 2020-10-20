#ifndef POSTOPERATION_H
#define POSTOPERATION_H

#include "qinstallerglobal.h"

#include <QtCore/QObject>
#include <QNetworkReply>

namespace QInstaller {

class INSTALLER_EXPORT PostOperation : public QObject, public Operation
{
    Q_OBJECT

public:
    explicit PostOperation(PackageManagerCore *core);

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();

private Q_SLOTS:
    void onError(QNetworkReply::NetworkError code);
    void onFinished();
};

} // namespace

#endif
