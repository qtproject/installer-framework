/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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
