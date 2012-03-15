#include "environment.h"

#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>

using namespace KDUpdater;

Environment &Environment::instance()
{
    static Environment s_instance;
    return s_instance;
}

QString Environment::value(const QString &key, const QString &defvalue) const
{
    const QHash<QString, QString>::ConstIterator it = m_tempValues.constFind(key);
    if (it != m_tempValues.constEnd())
        return *it;
    return QProcessEnvironment::systemEnvironment().value(key, defvalue);
}

void Environment::setTemporaryValue(const QString &key, const QString &value)
{
    m_tempValues.insert(key, value);
}

QProcessEnvironment Environment::applyTo(const QProcessEnvironment &qpe_) const
{
    QProcessEnvironment qpe(qpe_);
    QHash<QString, QString>::ConstIterator it = m_tempValues.constBegin();
    const QHash<QString, QString>::ConstIterator end = m_tempValues.constEnd();
    for ( ; it != end; ++it)
        qpe.insert(it.key(), it.value());
    return qpe;
}

void Environment::applyTo(QProcess *proc)
{
    proc->setProcessEnvironment(applyTo(proc->processEnvironment()));
}
