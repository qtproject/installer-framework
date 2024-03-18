/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Copyright (C) 2022 The Qt Company Ltd.
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

    enum OperationType {
        Backup,
        Perform,
        Undo
    };

    enum OperationGroup {
        Unpack = 0x1,
        Install = 0x2,
        All = (Unpack | Install),
        Default = Install
    };
    Q_DECLARE_FLAGS(OperationGroups, OperationGroup)

    explicit UpdateOperation(QInstaller::PackageManagerCore *core);
    virtual ~UpdateOperation();

    QString name() const;
    QString operationCommand() const;
    OperationGroup group() const;

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
    bool requiresUnreplacedVariables() const;

    QInstaller::PackageManagerCore *packageManager() const;

    virtual void backup() = 0;
    virtual bool performOperation() = 0;
    virtual bool undoOperation() = 0;
    virtual bool testOperation() = 0;

    virtual QDomDocument toXml() const;
    virtual bool fromXml(const QString &xml);
    virtual bool fromXml(const QDomDocument &doc);

    virtual quint64 sizeHint();

protected:
    void setName(const QString &name);
    void setGroup(const OperationGroup &group);
    void setErrorString(const QString &errorString);
    void setError(int error, const QString &errorString = QString());
    void registerForDelayedDeletion(const QStringList &files);
    bool deleteFileNowOrLater(const QString &file, QString *errorString = 0);
    bool checkArgumentCount(int minArgCount, int maxArgCount, const QString &argDescription = QString());
    bool checkArgumentCount(int argCount);
    QStringList parsePerformOperationArguments();
    bool skipUndoOperation();
    void setRequiresUnreplacedVariables(bool isRequired);
    bool variableReplacement(QString *variableValue);

private:
    QString m_name;
    OperationGroup m_group;
    QStringList m_arguments;
    QString m_errorString;
    int m_error;
    QVariantMap m_values;
    QStringList m_delayedDeletionFiles;
    QInstaller::PackageManagerCore *m_core;
    bool m_requiresUnreplacedVariables;
};

} // namespace KDUpdater

Q_DECLARE_METATYPE(KDUpdater::UpdateOperation *)

#endif // UPDATEOPERATION_H
