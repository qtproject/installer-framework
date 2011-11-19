#include "environment.h"

#include <QHash>
#include <QProcess>
#include <QProcessEnvironment>

using namespace KDUpdater;

class Environment::Private
{
public:
    static Environment s_instance;
    QHash<QString, QString> tempValues;
};

Environment Environment::Private::s_instance;

Environment::Environment()
    : d(new Private)
{
}

Environment::~Environment()
{
    delete d;
}

Environment &Environment::instance()
{
    return Private::s_instance;
}

QString Environment::value(const QString &key, const QString &defvalue) const
{
    const QHash<QString, QString>::ConstIterator it = d->tempValues.constFind(key);
    if (it != d->tempValues.constEnd())
        return *it;
    return QProcessEnvironment::systemEnvironment().value(key, defvalue);
}

void Environment::setTemporaryValue(const QString &key, const QString &value)
{
    d->tempValues.insert(key, value);
}

QProcessEnvironment Environment::applyTo(const QProcessEnvironment &qpe_) const
{
    QProcessEnvironment qpe(qpe_);
    QHash<QString, QString>::ConstIterator it = d->tempValues.constBegin();
    const QHash<QString, QString>::ConstIterator end = d->tempValues.constEnd();
    for ( ; it != end; ++it)
        qpe.insert(it.key(), it.value());
    return qpe;
}

void Environment::applyTo(QProcess *proc)
{
    proc->setProcessEnvironment(applyTo(proc->processEnvironment()));
}
