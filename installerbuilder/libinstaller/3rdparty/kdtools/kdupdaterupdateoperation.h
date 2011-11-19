/****************************************************************************
** Copyright (C) 2001-2010 Klaralvdalens Datakonsult AB.  All rights reserved.
**
** This file is part of the KD Tools library.
**
** Licensees holding valid commercial KD Tools licenses may use this file in
** accordance with the KD Tools Commercial License Agreement provided with
** the Software.
**
**
** This file may be distributed and/or modified under the terms of the
** GNU Lesser General Public License version 2 and version 3 as published by the
** Free Software Foundation and appearing in the file LICENSE.LGPL included.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
** Contact info@kdab.com if any conditions of this licensing are not
** clear to you.
**
**********************************************************************/

#ifndef KD_UPDATER_UPDATE_OPERATION_H
#define KD_UPDATER_UPDATE_OPERATION_H

#include "kdupdater.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVariant>
#include <QDomDocument>

namespace KDUpdater {

class Application;

class KDTOOLS_EXPORT UpdateOperation
{
    Q_DECLARE_TR_FUNCTIONS(UpdateOperation)

public:
    enum Error {
        NoError = 0,
        InvalidArguments = 1,
        UserDefinedError = 128
    };

    UpdateOperation();
    virtual ~UpdateOperation();

    QString name() const;
    QString operationCommand() const;

    bool hasValue(const QString &name) const;
    void clearValue(const QString &name);
    QVariant value(const QString &name) const;
    void setValue(const QString &name, const QVariant &value);

    void setArguments(const QStringList &args);
    void setApplication(Application *application);
    QStringList arguments() const;
    void clear();
    QString errorString() const;
    int error() const;
    QStringList filesForDelayedDeletion() const;

    virtual void backup() = 0;
    virtual bool performOperation() = 0;
    virtual bool undoOperation() = 0;
    virtual bool testOperation() = 0;
    virtual UpdateOperation *clone() const = 0;

    virtual QDomDocument toXml() const;
    virtual bool fromXml(const QString &xml);
    virtual bool fromXml(const QDomDocument &doc);

protected:
    void setName(const QString &name);
    Application *application() const;
    void setErrorString(const QString &errorString);
    void setError(int error, const QString &errorString = QString());
    void registerForDelayedDeletion(const QStringList &files);
    bool deleteFileNowOrLater(const QString &file, QString *errorString = 0);

private:
    struct UpdateOperationData;
    UpdateOperationData *d;
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_OPERATION_H
