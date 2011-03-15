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
#ifndef QINSTALLER_COMPONENTMODEL_H
#define QINSTALLER_COMPONENTMODEL_H

#include <QAbstractItemModel>
#include <QList>

#include "installer_global.h"

namespace QInstaller {
class Installer;
class Component;

class INSTALLER_EXPORT ComponentModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    enum Role {
        ComponentRole=Qt::UserRole,
        IdRole
    };

    enum Column {
        NameColumn=0,
        InstalledVersionColumn,
        VersionColumn,
        SizeColumn
    };

    explicit ComponentModel( Installer* parent, RunModes runMode = InstallerMode);
    ~ComponentModel();

    QModelIndex index( int row, int column, const QModelIndex& parent = QModelIndex() ) const;
    QModelIndex parent( const QModelIndex& index ) const;

    int columnCount( const QModelIndex& parent = QModelIndex() ) const;
    int rowCount( const QModelIndex& parent = QModelIndex() ) const;

    Qt::ItemFlags flags( const QModelIndex& index ) const;

    bool setData( const QModelIndex& index, const QVariant& data, int role = Qt::CheckStateRole );
    QVariant data( const QModelIndex& index, int role = Qt::DisplayRole ) const;
    QVariant headerData( int section, Qt::Orientation orientation, int role = Qt::DisplayRole ) const;

    static bool virtualComponentsVisible();
    static void setVirtualComponentsVisible( bool visible );
    
    static QFont virtualComponentsFont();
    static void setVirtualComponentsFont( const QFont& f );

    QModelIndex findComponent( const QString& id ) const;
Q_SIGNALS:
    void workRequested( bool value );

public Q_SLOTS:
    void addRootComponents(QList< QInstaller::Component* > components );
    void clear();

private Q_SLOTS:
    void componentAdded( QInstaller::Component* comp );
    void selectedChanged( bool checked );

private:
    mutable QList< Component* > seenComponents;
    QList< Component* > m_components;
    RunModes m_runMode;
};

}

#endif // QINSTALLER_COMPONENTMODEL_H
