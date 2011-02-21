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
#ifndef COPYDIRECTORYOPERATION_H
#define COPYDIRECTORYOPERATION_H

#include <KDUpdater/UpdateOperation>

#include "installer_global.h"
#include <QtCore/QObject>
#include <QFuture>


namespace QInstaller {

class INSTALLER_EXPORT CopyDirectoryOperation : public QObject, public KDUpdater::UpdateOperation
{
    Q_OBJECT

    friend class WorkerThread;
public:
    CopyDirectoryOperation();
    ~CopyDirectoryOperation();

    void backup();
    bool performOperation();
    bool undoOperation();
    bool testOperation();
    CopyDirectoryOperation* clone() const;

Q_SIGNALS:
    void outputTextChanged( const QString& progress );
    //TODO: needs progress signal

private Q_SLOTS:
    
private:
    void addFileToFileList( const QString& fileName );
    void addDirectoryToDirectoryList( const QString& directory );
    void letTheUiRunTillFinished(const QFuture<void> &_future);
    bool copyFilesFromInternalFileList();
    bool removeFilesFromInternalFileList();
    QList< QPair<QString,QString> > fileList();
    QList< QPair<QString,QString> > generateFileList(const QString &sourcePath, const QString &targetPath);
    QList< QPair<QString,QString> > m_fileList;
};

}

#endif
