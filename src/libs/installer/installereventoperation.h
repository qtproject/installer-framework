#ifndef INSTALLEREVENTOPERATION_H
#define INSTALLEREVENTOPERATION_H

#include "qinstallerglobal.h"

#include <QtCore/QObject>

namespace QInstaller {

class INSTALLER_EXPORT InstallerEventOperation : public QObject, public Operation
{
    Q_OBJECT

public:
    explicit InstallerEventOperation(PackageManagerCore *core);

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
private:
    bool sendInit(QStringList args);
    bool sendInstallerEvent(QStringList args);
    bool sendUninstallerEvent(QStringList args);
    bool m_initSuccessful;
};

} // namespace

#endif
