#ifndef FAKEINSTALLER_H
#define FAKEINSTALLER_H

#include <qinstaller.h>

#include <QObject>
#include <QMetaType>
#include <QString>

class FakeInstaller : public QInstaller::Installer
{
    Q_OBJECT
public:
    FakeInstaller() : QInstaller::Installer() {}
    void setTargetDir(const QString &targetDir);
    Q_INVOKABLE virtual QString value(const QString &key, const QString &defaultValue = QString()) const;

private:
    QString m_targetDir;
};

Q_DECLARE_METATYPE(FakeInstaller*)

#endif // FAKEINSTALLER_H
