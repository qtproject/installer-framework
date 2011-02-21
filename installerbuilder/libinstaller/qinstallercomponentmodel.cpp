/**************************************************************************
**
** This file is part of Qt SDK**
**
** Copyright (c) 2011 Nokia Corporation and/or its subsidiary(-ies).*
**
** Contact:  Nokia Corporation qt-info@nokia.com**
**
** No Commercial Usage
**
** This file contains pre-release code and may not be distributed.
** You may use this file in accordance with the terms and conditions
** contained in the Technology Preview License Agreement accompanying
** this package.
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
#include "qinstallercomponentmodel.h"

#include <algorithm>

#include "qinstallercomponent.h"
#include "qinstallerglobal.h"

#include "common/utils.h"

#include <QApplication>
#include <QFont>
#include <QScriptEngine>

using namespace QInstaller;

struct ComponentIsVirtual
{
    bool operator()( const Component* comp ) const
    {
        return comp->value( QLatin1String( "Virtual" ), QLatin1String( "false" ) ).toLower() == QLatin1String( "true" );
    }
};

struct ComponentPriorityLessThan
{
    bool operator()( const Component* lhs, const Component* rhs ) const
    {
        return lhs->value( QLatin1String( "SortingPriority" ) ).toInt() <
               rhs->value( QLatin1String( "SortingPriority" ) ).toInt();
    }
};

bool checkCompleteUninstallation( const QInstaller::Component* component )
{
    const QList< QInstaller::Component* > components = component->components( true );
    bool nonSelected = true;
    Q_FOREACH( const QInstaller::Component* comp, components )
    {
        if ( comp->isSelected() )
            nonSelected = false;
        if ( !checkCompleteUninstallation( comp ) )
            nonSelected = false;
    }
    return nonSelected;
}

bool checkWorkRequest( const QInstaller::Component* component )
{
    //first check the component itself
    QString componentName = component->name();
    QString wantedState = component->value( QLatin1String( "WantedState" ) );
    QString currentState = component->value( QLatin1String( "CurrentState" ) );
    if (!wantedState.isEmpty() && !currentState.isEmpty() && wantedState != currentState ) {
        verbose() << QLatin1String("request work for ") << componentName << QString(QLatin1String(" because WantedState(%1)!=CurrentState(%2)")).arg(wantedState, currentState)<<std::endl;
        return true;
    }

    //now checkWorkRequest for all childs
    const QList< QInstaller::Component* > components = component->components( true );
    Q_FOREACH( const QInstaller::Component* currentComponent, components )
    {
        if ( checkWorkRequest( currentComponent ) )
            return true;
    }
    return false;
}

ComponentModel::ComponentModel( Installer* parent, RunModes runMode )
    : QAbstractItemModel( parent ), m_runMode( runMode )
{

    if ( runMode == InstallerMode )
        connect( parent, SIGNAL( componentsAdded( QList< QInstaller::Component* > ) ), this, SLOT( addComponents(QList< QInstaller::Component* > ) ) );
    else
        connect( parent, SIGNAL( updaterComponentsAdded( QList< QInstaller::Component* > ) ), this, SLOT( addComponents(QList< QInstaller::Component* > ) ) );
    connect( parent, SIGNAL( componentsAboutToBeCleared() ), this, SLOT( clear() ) );
    connect( parent, SIGNAL( componentAdded( QInstaller::Component* ) ), this, SLOT( componentAdded( QInstaller::Component* ) ) );
}

void ComponentModel::addComponents( QList< Component* > components )
{
    beginResetModel();
    bool requestWork = false;
    Q_FOREACH( Component* currentComponent, components )
    {
        if ( !this->components.contains( currentComponent ) )
            this->components.push_back( currentComponent );
        if ( checkWorkRequest( currentComponent ) ) {
            requestWork = true;
            break;
        }
    }
    endResetModel();
    emit workRequested( requestWork );
}
void ComponentModel::clear()
{
    beginResetModel();
    components.clear();
    seenComponents.clear();
    endResetModel();
}

ComponentModel::~ComponentModel()
{
}

void ComponentModel::selectedChanged( bool checked )
{
    Q_UNUSED( checked )
    Component *comp = dynamic_cast<Component*>( QObject::sender() );
    Q_ASSERT( comp );
    bool requestWork = false;
    Q_FOREACH( Component *c, comp->components( false, m_runMode ) ) {
        if( !seenComponents.contains( c ) )
        {
            connect( c, SIGNAL( selectedChanged( bool ) ), this, SLOT( selectedChanged( bool ) ) );
            seenComponents.push_back( c );            
        }        
    }
    Q_FOREACH( const Component * currentComponent, components )
    {
        if ( checkWorkRequest( currentComponent ) ) {
            requestWork = true;
            break;
        }
    }
    emit workRequested( requestWork );
    emit dataChanged( this->index( 0, 0 ), QModelIndex() );
}

void ComponentModel::componentAdded( Component* comp )
{
    Q_UNUSED( comp )
    reset();
}

/*!
 \reimpl
*/
QModelIndex ComponentModel::index( int row, int column, const QModelIndex& parent ) const
{
    if( row < 0 || row >= rowCount( parent ) )
        return QModelIndex();
    if( column < 0 || column >= columnCount( parent ) )
        return QModelIndex();

    QList< Component* > components = !parent.isValid() ? this->components
                                                         : reinterpret_cast< Component* >( parent.internalPointer() )->components( false, m_runMode );
    // don't count virtual components
    if( !virtualComponentsVisible() && m_runMode == InstallerMode )
        components.erase( std::remove_if( components.begin(), components.end(), ComponentIsVirtual() ), components.end() );

    // sort by priority
    std::sort( components.begin(), components.end(), ComponentPriorityLessThan() );

    Component* const comp = components[ row ];

    QModelIndex index = createIndex( row, column, comp );

    if( !seenComponents.contains( comp ) )
    {
        connect( comp, SIGNAL( selectedChanged( bool ) ), this, SLOT( selectedChanged( bool ) ) );
        seenComponents.push_back( comp );
    }

    return index;
}

/*!
 \reimpl
*/
QModelIndex ComponentModel::parent( const QModelIndex& index ) const
{
    if( !index.isValid() )
        return QModelIndex();
    const Component* const component = reinterpret_cast< Component* >( index.internalPointer() );
    Component* const parentComponent = component->parentComponent( m_runMode );
    if( parentComponent == 0 )
        return QModelIndex();
    QList< Component* > parentSiblings = parentComponent->parentComponent() == 0 ? this->components
                                                                                 : parentComponent->parentComponent()->components( false, m_runMode );
    // don't count virtual components
    if( !virtualComponentsVisible() && m_runMode == InstallerMode )
        parentSiblings.erase( std::remove_if( parentSiblings.begin(), parentSiblings.end(), ComponentIsVirtual() ), parentSiblings.end() );

    // sort by priority
    std::sort( parentSiblings.begin(), parentSiblings.end(), ComponentPriorityLessThan() );

    return createIndex( parentSiblings.indexOf( parentComponent ), 0, parentComponent );
}

/*!
 \reimpl
*/
int ComponentModel::columnCount( const QModelIndex& parent ) const
{
    Q_UNUSED( parent );
    return 4;
}

/*!
 \reimpl
*/
int ComponentModel::rowCount( const QModelIndex& parent ) const
{
    if( parent.column() > 0 )
        return 0;

    QList< Component* > components = !parent.isValid() ? this->components
                                                         : reinterpret_cast< Component* >( parent.internalPointer() )->components( false, m_runMode );

    // don't count virtual components
    if( !virtualComponentsVisible() && m_runMode == InstallerMode )
        components.erase( std::remove_if( components.begin(), components.end(), ComponentIsVirtual() ), components.end() );

    return components.count();
}

/*!
 \reimpl
*/
Qt::ItemFlags ComponentModel::flags( const QModelIndex& index ) const
{
    Qt::ItemFlags result = QAbstractItemModel::flags( index );
    if ( !index.isValid() )
        return result;
    const Component* const component = reinterpret_cast< Component* >( index.internalPointer() );
    const bool forcedInstallation = QVariant( component->value( QLatin1String( "ForcedInstallation" ) ) ).toBool();

    if (m_runMode == UpdaterMode || !forcedInstallation)
        result |= Qt::ItemIsUserCheckable;
    if ( !component->isEnabled() )
        result &= Qt::ItemIsDropEnabled;
    if (m_runMode == InstallerMode && forcedInstallation) {
        result &= ~Qt::ItemIsEnabled; //now it should look like a disabled item
    }
    return result;
}

/*!
 \reimpl
*/
bool ComponentModel::setData( const QModelIndex& index, const QVariant& data, int role )
{
    if( !index.isValid() )
        return false;

    Component* const component = reinterpret_cast< Component* >( index.internalPointer() );
    switch( role )
    {
    case Qt::CheckStateRole: {
        if( !( flags( index ) & Qt::ItemIsUserCheckable ) )
            return false;
        const bool check = data.toInt() == Qt::Checked;
        component->setSelected( check, m_runMode );
        bool requestWork = false;
        bool nonSelected = true;
        Q_FOREACH( const QInstaller::Component* currentComponent, components )
        {
            if ( checkWorkRequest( currentComponent ) ) {
                requestWork |= true;
            }
            if ( currentComponent->isSelected() )
                nonSelected = false;
            if ( !checkCompleteUninstallation( currentComponent ) )
                nonSelected = false;
        }
        if ( m_runMode == InstallerMode )
        {
            Installer* installer = dynamic_cast< Installer* > ( QObject::parent() );
            installer->setCompleteUninstallation( nonSelected );
        }
        emit workRequested( requestWork );
        return true;
    }
    default:
        return false;
    }
}

Qt::CheckState QInstaller::componentCheckState( const Component* component, RunModes runMode )
{
    if( QVariant( component->value( QLatin1String( "ForcedInstallation" ) ) ).toBool() )
        return Qt::Checked;

    QString autoSelect = component->value( QLatin1String( "AutoSelectOn" ) );
    // check the auto select expression
    if( !autoSelect.isEmpty() )
    {
        QScriptEngine engine;
        const QList< Component* > components = component->installer()->components( true, runMode );
        for( QList< Component* >::const_iterator it = components.begin(); it != components.end(); ++it )
        {
            const Component* const c = *it;
            if( c == component )
                continue;
            QString name = c->name();
            name.replace( QLatin1String( "." ), QLatin1String( "___" ) );
            engine.evaluate( QString::fromLatin1( "%1 = %2;" ).arg( name, QVariant( c ->isSelected() ).toString() ) );
        }
        autoSelect.replace( QLatin1String( "." ), QLatin1String( "___" ) );
        if( engine.evaluate( autoSelect ).toBool() )
            return Qt::Checked;
    }

    // if one of our dependees is checked, we are checked
    const QList< Component* > dependees = component->dependees();
    for( QList< Component* >::const_iterator it = dependees.begin(); it != dependees.end(); ++it )
    {
        if( *it == component )
        {
            verbose() << "Infinite loop in dependencies detected, bailing out..." << std::endl;
            return Qt::Unchecked;
        }
        const Qt::CheckState state = (*it)->checkState();
        if( state == Qt::Checked )
            return state;
    }
    
    // if the component has children then we need to check if all children are selected
    // to set the state to either checked if all children are selected or to unchecked
    // if no children are selected or otherwise to partially checked.
    bool foundChecked = false;
    bool foundUnchecked = false;
    QList< Component* > children = component->components( true, runMode );
    // don't count virtual components
    children.erase( std::remove_if( children.begin(), children.end(), ComponentIsVirtual() ), children.end() );
    if( !children.isEmpty() )
    {
        for( QList< Component* >::const_iterator it = children.begin(); it != children.end(); ++it )
        {
            const Qt::CheckState state = (*it)->checkState();
            foundChecked = foundChecked || state == Qt::Checked || state == Qt::PartiallyChecked;
            foundUnchecked = foundUnchecked || state == Qt::Unchecked;
        }
        if( foundChecked && foundUnchecked )
            return Qt::PartiallyChecked;
        else if( foundChecked && ! foundUnchecked )
            return Qt::Checked;
        else if( foundUnchecked && ! foundChecked )
            return Qt::Unchecked;
        //else fall through
    }
    
    // explicitely selected
    if( component->value( QString::fromLatin1( "WantedState" ) ) == QString::fromLatin1( "Installed" ) )
        return Qt::Checked;

    // explicitely unselected
    else if ( component->value( QString::fromLatin1( "WantedState" ) ) == QString::fromLatin1( "Uninstalled" ) )
        return Qt::Unchecked;
    
    // no decision made, use the predefined:
    const QString suggestedState = component->value( QString::fromLatin1( "SuggestedState" ) );
    if( suggestedState == QString::fromLatin1( "Installed" ) )
        return Qt::Checked;
    return Qt::Unchecked;
}

/*!
 \reimp
*/
QVariant ComponentModel::data( const QModelIndex& index, int role ) const
{
    if( !index.isValid() )
        return QVariant();
        
    Component* const component = reinterpret_cast< Component* >( index.internalPointer() );

    switch( index.column() )
    {
    case NameColumn:
        switch( role )
        {
        case Qt::EditRole:
        case Qt::DisplayRole:
            return component->displayName();
        case Qt::CheckStateRole:
            return component->checkState( m_runMode );
        case Qt::ToolTipRole:
            return component->value( QString::fromLatin1( "Description" ) ) + QLatin1String( "<br><br> Update Info: " ) + component->value( QLatin1String ( "UpdateText" ) ) ;
        case Qt::FontRole:
            return component->value( QLatin1String( "Virtual" ), QLatin1String( "false" ) ).toLower() == QLatin1String( "true" ) ? virtualComponentsFont() : QFont();
        case ComponentRole:
            return qVariantFromValue( component );
        case IdRole:
            return component->name();
        default:
            return QVariant();
        }
    case VersionColumn:
        if( role == Qt::DisplayRole )
            return component->value( QLatin1String( "Version" ) );
        break;
    case InstalledVersionColumn:
        if( role == Qt::DisplayRole )
            return component->value( QLatin1String( "InstalledVersion" ) );
        break;
    case SizeColumn:
        if( role == Qt::DisplayRole )
        {
            double size = component->value( QLatin1String( "UncompressedSize" ) ).toDouble();
            if( size < 10000.0 )
                return tr( "%L1 Bytes" ).arg( size );
            size /= 1024.0;
            if( size < 10000.0 )
                return tr( "%L1 kB" ).arg( size, 0, 'f', 1 );
            size /= 1024.0;
            if( size < 10000.0 )
                return tr( "%L1 MB" ).arg( size, 0, 'f', 1 );
            size /= 1024.0;
            return tr( "%L1 GB" ).arg( size, 0, 'f', 1 );
        }
        break;
    }
    return QVariant();
}

/*!
 \reimp
*/
QVariant ComponentModel::headerData( int section, Qt::Orientation orientation, int role ) const
{
    if( orientation != Qt::Horizontal || role != Qt::DisplayRole )
        return QAbstractItemModel::headerData( section, orientation, role );
    
    switch( section )
    {
    case NameColumn:
        return tr( "Name" );
    case InstalledVersionColumn:
        return tr( "Installed Version" );
    case VersionColumn:
        return tr( "New Version" );
    case SizeColumn:
        return tr( "Size" );
    default:
        return QAbstractItemModel::headerData( section, orientation, role );
    }
}

static bool s_virtualComponentsVisible = false;

bool ComponentModel::virtualComponentsVisible()
{
    return s_virtualComponentsVisible;
}

void ComponentModel::setVirtualComponentsVisible( bool visible )
{
    s_virtualComponentsVisible = visible;
}

static QFont s_virtualComponentsFont;

QFont ComponentModel::virtualComponentsFont()
{
    return s_virtualComponentsFont;
}

void ComponentModel::setVirtualComponentsFont( const QFont& f )
{
    s_virtualComponentsFont = f;
}

QModelIndex ComponentModel::findComponent( const QString& id ) const
{
    QModelIndex idx = index( 0, NameColumn );
    while ( idx.isValid() && idx.data( IdRole ).toString() != id ) {
        if ( rowCount( idx ) > 0 ) {
            idx = idx.child( 0, NameColumn );
            continue;
        }

        bool foundNext = false;

        while ( !foundNext ) {
            const int oldRow = idx.row();
            idx = idx.parent();
            const int rc = rowCount( idx );
            if ( oldRow < rc - 1 ) {
                idx = index( oldRow + 1, NameColumn, idx );
                foundNext = true;
            } else if ( !idx.isValid() )
                return idx;
        }
    }
    return idx;
}
