#ifndef LIBINSTALLER_ENVIRONMENT_H
#define LIBINSTALLER_ENVIRONMENT_H

#include "kdtoolsglobal.h"

#include <QString>
#include <QHash>

QT_BEGIN_NAMESPACE
class QProcess;
class QProcessEnvironment;
QT_END_NAMESPACE

namespace KDUpdater {

class KDTOOLS_EXPORT Environment
{
public:
    static Environment &instance();

    ~Environment() {}

    QString value(const QString &key, const QString &defaultValue = QString()) const;
    void setTemporaryValue(const QString &key, const QString &value);

    QProcessEnvironment applyTo(const QProcessEnvironment &qpe) const;
    void applyTo(QProcess *process);

private:
    Environment() {}

private:
    Q_DISABLE_COPY(Environment)
    QHash<QString, QString> m_tempValues;
};

} // namespace KDUpdater

#endif
