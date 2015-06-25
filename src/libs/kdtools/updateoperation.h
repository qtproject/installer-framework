/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef UPDATEOPERATION_H
#define UPDATEOPERATION_H

#include "updater.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVariant>
#include <QtXml/QDomDocument>

namespace QInstaller {
class PackageManagerCore;
}

namespace KDUpdater {

class KDTOOLS_EXPORT UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(UpdateOperation)

public:
    enum Error {
        NoError = 0,
        InvalidArguments = 1,
        UserDefinedError = 128
    };

    explicit UpdateOperation(QInstaller::PackageManagerCore *core);
    virtual ~UpdateOperation();

    QString name() const;
    QString operationCommand() const;

    bool hasValue(const QString &name) const;
    void clearValue(const QString &name);
    QVariant value(const QString &name) const;
    void setValue(const QString &name, const QVariant &value);

    void setArguments(const QStringList &args);
    QStringList arguments() const;
    QString argumentKeyValue(const QString & key, const QString &defaultValue = QString()) const;
    void clear();
    QString errorString() const;
    int error() const;
    QStringList filesForDelayedDeletion() const;

    QInstaller::PackageManagerCore *packageManager() const;

    virtual void backup() = 0;
    virtual bool performOperation() = 0;
    virtual bool undoOperation() = 0;
    virtual bool testOperation() = 0;

    virtual QDomDocument toXml() const;
    virtual bool fromXml(const QString &xml);
    virtual bool fromXml(const QDomDocument &doc);

protected:
    void setName(const QString &name);
    void setErrorString(const QString &errorString);
    void setError(int error, const QString &errorString = QString());
    void registerForDelayedDeletion(const QStringList &files);
    bool deleteFileNowOrLater(const QString &file, QString *errorString = 0);
    bool checkArgumentCount(int minArgCount, int maxArgCount, const QString &argDescription = QString());
    bool checkArgumentCount(int argCount);

private:
    QString m_name;
    QStringList m_arguments;
    QString m_errorString;
    int m_error;
    QVariantMap m_values;
    QStringList m_delayedDeletionFiles;
    QInstaller::PackageManagerCore *m_core;
};

} // namespace KDUpdater

#endif // UPDATEOPERATION_H
