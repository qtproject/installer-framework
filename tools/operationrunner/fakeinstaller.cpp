#include "fakeinstaller.h"

#include <QFileInfo>

void FakeInstaller::setTargetDir(const QString &targetDir)
{
    m_targetDir = QFileInfo(targetDir).absoluteFilePath();
}

QString FakeInstaller::value(const QString &key, const QString &/*defaultValue*/) const
{
    if(key == QLatin1String("TargetDir")) {
        return m_targetDir;
    } else {
        qFatal("This is only a fake installer and it can only handle \"TargetDir\" value.");
    }
    return QString();
}
