/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef KD_UPDATER_UPDATE_OPERATION_H
#define KD_UPDATER_UPDATE_OPERATION_H

#include "kdupdater.h"

#include <QCoreApplication>
#include <QStringList>
#include <QVariant>
#include <QtXml/QDomDocument>

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

    UpdateOperation();
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
    void setErrorString(const QString &errorString);
    void setError(int error, const QString &errorString = QString());
    void registerForDelayedDeletion(const QStringList &files);
    bool deleteFileNowOrLater(const QString &file, QString *errorString = 0);

private:
    QString m_name;
    QStringList m_arguments;
    QString m_errorString;
    int m_error;
    QVariantMap m_values;
    QStringList m_delayedDeletionFiles;
};

} // namespace KDUpdater

#endif // KD_UPDATER_UPDATE_OPERATION_H
