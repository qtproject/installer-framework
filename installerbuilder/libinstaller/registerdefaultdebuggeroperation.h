/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** GNU Lesser General Public License Usage
**
** This file may be used under the terms of the GNU Lesser General Public
** License version 2.1 as published by the Free Software Foundation and
** appearing in the file LICENSE.LGPL included in the packaging of this file.
** Please review the following information to ensure the GNU Lesser General
** Public License version 2.1 requirements will be met:
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Nokia gives you certain additional
** rights. These rights are described in the Nokia Qt LGPL Exception version
** 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** If you are unsure which license is appropriate for your use, please contact
** (qt-info@nokia.com).
**
**************************************************************************/
#ifndef REGISTERDEFAULTDEBUGGEROPERATION_H
#define REGISTERDEFAULTDEBUGGEROPERATION_H

//#include <KDUpdater/UpdateOperation>

#include <QObject>
#include <QStringList>
typedef QObject KDUpdaterUpdateOperation;

namespace QInstaller {

class RegisterDefaultDebuggerOperation : public KDUpdaterUpdateOperation
{
public:
    RegisterDefaultDebuggerOperation();
    ~RegisterDefaultDebuggerOperation();
    enum error {
        UserDefinedError,
        InvalidArguments
    };

    void setName(const QString &) {}
    QString name() {return "something";}
    void setError(error) {}
    void setErrorString(const QString &) {}

    void setArguments(const QStringList &arguments) {m_arguments = arguments;}
    QStringList arguments() {return m_arguments;}
    QStringList m_arguments;

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    KDUpdaterUpdateOperation* clone() const;
};

} // namespace

#endif // REGISTERDEFAULTDEBUGGEROPERATION_H
