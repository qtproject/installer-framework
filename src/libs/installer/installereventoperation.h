#ifndef INSTALLEREVENTOPERATION_H
#define INSTALLEREVENTOPERATION_H

#include "qinstallerglobal.h"
#include "eventlogger.h"

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
    eve_launcher::uninstaller::Page toUninstallerPage(bool *ok, QString value);
    eve_launcher::installer::Page toInstallerPage(bool *ok, QString value);
    eve_launcher::installer::Component toInstallerComponent(bool *ok, QString value);
    eve_launcher::installer::RedistVersion toInstallerRedistVersion(bool *ok, QString value);
};
} // namespace

#endif
