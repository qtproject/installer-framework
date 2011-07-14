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

#include "kdmetamethoditerator.h"

/*!
 \class KDMetaMethodIterator
 \ingroup core
 \brief Iterator over methods of QObjects
 \since_c 2.3

 KDMetaMethodIterator provides a way over all methods of a QObject which can be accessed via Qt's meta
 object system. It is possible to filter on types of methods (Q_INVOKABLE methods, signals, slots, 
 constructors) and on access types (public, protected, private). Furthermore, KDMetaMethodIterator can
 be configured to filter methods of the given object's base class or all methods provided by QObject.

 \section general-use General Use

 The following example shows a general use case of KDMetaMethodIterator:

 \code

 class QIODeviceWrapper : public QIODevice
 {
     Q_OBJECT
 public:
     QIODeviceWrapper( QIODevice* nestedDevice )
        : nested( nestedDevice )
    {
        // make sure all signals of the nested QIODevice are forwarded,
        // but make sure not to connect QObject's own destroyed() signal,
        // therefore use KDMetaMethodIterator::IgnoreQObjectMethods
        KDMetaMethodIterator it( QIODevice::staticMetaObject, KDMetaMethodIterator::Signal, KDMetaMethodIterator::IgnoreQObjectMethods );
        while( it.hasNext() )
        {
            it.next();
            connect( nested, it.connectableSignature(), this, it.connectableSignature() );
        }
    }

 protected:
    // virtual methods needed to access the contents...

 private:
     QIODevice* const nested;
 };

 \endcode
*/

/*!
 \enum KDMetaMethodIterator::AccessType
 This enum describes the access type of methods to iterate:
*/

/*!
 \var KDMetaMethodIterator::Private
 Iterate over private methods.
*/

/*!
 \var KDMetaMethodIterator::Protected
 Iterate over protected methods.
*/

/*!
 \var KDMetaMethodIterator::Public
 Iterate over public methods.
*/

/*!
 \var KDMetaMethodIterator::AllAccessTypes
 Iterate over methods of all access types.
*/

/*!
 \enum KDMetaMethodIterator::MethodType
 This enum describes the type of methods to iterate:
*/

/*!
 \var KDMetaMethodIterator::Method
 Iterate over methods marked with Q_INVOKABLE
*/

/*!
 \var KDMetaMethodIterator::Signal
 Iterate over signals.
*/

/*!
 \var KDMetaMethodIterator::Slot
 Iterate over slots.
*/

/*!
 \var KDMetaMethodIterator::Constructor
 Iterate over constructors marked with Q_INVOKABLE.
*/

/*!
 \var KDMetaMethodIterator::AllMethodTypes
 Iterate over all supported method types.
*/

/*!
 \enum KDMetaMethodIterator::IteratorFlag
 This enum describes flags which can be passed to KDMetaMethodIterator:
*/

/*!
 \var KDMetaMethodIterator::NoFlags
 No flags set, only type filtering takes place.
*/

/*!
 \var KDMetaMethodIterator::IgnoreQObjectMethods
 KDMetaMethodIterator will not iterate over methods provided by QObject itself.
*/

/*!
 \var KDMetaMethodIterator::IgnoreSuperClassMethods
 KDMetaMethodIterator will only iterate over methods provided by the passed class instance itself, ignoring any base classes.
*/

/*!
 \internal
 */
class KDMetaMethodIterator::Priv
{
public:
    Priv( const QMetaObject* object, MethodTypes types, AccessTypes access, IteratorFlags flags )
        : index( -1 ),
          m( types ),
          a( access ),
          f( flags ),
          metaObject( object )
    {
    }

    Priv( const QMetaObject* object, MethodTypes types, IteratorFlags flags )
        : index( -1 ),
          m( types ),
          a( ( types & Signal ) ? ( Protected | Public ) : Public ),
          f( flags ),
          metaObject( object )
    {
    }

    bool filterMatches( const QMetaMethod& method, int index ) const;

    int index;
    const MethodTypes m;
    const AccessTypes a;
    const IteratorFlags f;
    const QMetaObject* const metaObject;

    mutable QByteArray connectableSignature;
};

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods of the given \a types.
 By default, only public methods will be returned. If \a types contains Signal, the access type filter will automatically set 
 to return protected and public. 
 
 By default, \a flags is NoFlags, which lists all methods.
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject& metaObject, MethodTypes types, IteratorFlags flags )
    : d( new Priv( &metaObject, types, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods of the given \a types,
 matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject& metaObject, MethodTypes types, AccessType access, IteratorFlags flags )
    : d( new Priv( &metaObject, types, access, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject& metaObject, AccessType access, IteratorFlags flags )
    : d( new Priv( &metaObject, AllMethodTypes, access, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods of the given \a types.
 By default, only public methods will be returned. If \a types contains Signal, the access type filter will automatically set 
 to return protected and public. 
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject* metaObject, MethodTypes types, IteratorFlags flags )
    : d( new Priv( metaObject, types, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods of the given \a types,
 matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject* metaObject, MethodTypes types, AccessType access, IteratorFlags flags )
    : d( new Priv( metaObject, types, access, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a metaObject. The iterator will only return methods matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QMetaObject* metaObject, AccessType access, IteratorFlags flags )
    : d( new Priv( metaObject, AllMethodTypes, access, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a object. The iterator will only return methods of the given \a types.
 By default, only public methods will be returned. If \a types contains Signal, the access type filter will automatically set 
 to return protected and public. 
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QObject* object, MethodTypes types, IteratorFlags flags )
    : d( new Priv( object->metaObject(), types, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a object. The iterator will only return methods of the given \a types,
 matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QObject* object, MethodTypes types, AccessType access, IteratorFlags flags )
    : d( new Priv( object->metaObject(), types, access, flags ) )
{
}

/*!
 Creates a new KDMetaMethodIterator iterating over \a object. The iterator will only return methods matching the \a access mode.
 
 By default, \a flags is NoFlags, which lists all methods.

 \overload
 */
KDMetaMethodIterator::KDMetaMethodIterator( const QObject* object, AccessType access, IteratorFlags flags )
    : d( new Priv( object->metaObject(), AllMethodTypes, access, flags ) )
{
}

/*!
 Destroys the KDMetaMethodIterator.
 */
KDMetaMethodIterator::~KDMetaMethodIterator()
{
}

/*!
 Returns true if there is at least one item ahead of the iterator, i.e. the iterator is not at the back of the method list; otherwise returns false.
 */
bool KDMetaMethodIterator::hasNext() const
{
    for( int i = d->index + 1; i < d->metaObject->methodCount(); ++i )
    {
        const QMetaMethod method = d->metaObject->method( i );
        if( d->filterMatches( method, i ) )
            return true;
    }
    return false;
}

/*!
 Returns the next method and advances the iterator by one position.
 */
QMetaMethod KDMetaMethodIterator::next()
{
    Q_ASSERT( hasNext() );
    d->connectableSignature.clear();
    for( ++d->index; d->index < d->metaObject->methodCount(); ++d->index )
    {
        const QMetaMethod method = d->metaObject->method( d->index );
        if( d->filterMatches( method, d->index ) )
            return method;
    }
    return QMetaMethod();
}

/*!
 Returns the current method's signature ready to be passed to QObject::connect, i.e. already passed through the SIGNAL or SLOT macro.
 The return value stays valid up to the following next() call. This is only valid for signals and slots.
 */
const char* KDMetaMethodIterator::connectableSignature() const
{
    if( d->connectableSignature.isEmpty() )
    {
        const QMetaMethod method = d->metaObject->method( d->index );
        d->connectableSignature = method.signature();
        d->connectableSignature.push_front( QString::number( ( static_cast< int >( method.methodType() ) - 3 ) * -1 ).toLocal8Bit() );
    }
    return d->connectableSignature.constData();
}

/*!
 Returns true if \a method matches the filters set on the iterator, currently pointing to \a index.
 \internal
 */
bool KDMetaMethodIterator::Priv::filterMatches( const QMetaMethod& method, int index ) const
{
    // filter QObject's own methods
    if( ( f & IgnoreQObjectMethods ) && index < QObject::staticMetaObject.methodCount() )
        return false;
    
    // filter any super classes methods
    if( ( f & IgnoreSuperClassMethods ) && index < metaObject->methodOffset() )
        return false;

    // filter any methods with unwanted access types
    if( ( ( 1 << method.access() ) & a ) == 0 )
        return false;

    // filter any methods with unwanted method types
    return ( ( 1 << method.methodType() ) & m ) != 0;
}

//#ifdef KDTOOLSCORE_UNITTESTS

//#include <KDUnitTest/Test>

//class TestClass : public QObject
//{
//    Q_OBJECT
//public:
//    TestClass(){}

//public Q_SLOTS:
//    void publicSlot( int ) {}

//protected Q_SLOTS:
//    void protectedSlot( int ) {}

//private Q_SLOTS:
//    void privateSlot( int ) {}

//private:
//    Q_PRIVATE_SLOT( d, void veryPrivateSlot() )
    
//    class Private;
//    kdtools::pimpl_ptr< Private > d;
//};

//class TestClassDerived : public TestClass
//{
//    Q_OBJECT
//public:
//    TestClassDerived(){}
//};

//class TestClass::Private
//{
//public:
//    void veryPrivateSlot() {}
//};

//KDAB_UNITTEST_SIMPLE( KDMetaMethodIterator, "kdcoretools" ) {

//    QObject o;
//    {
//        KDMetaMethodIterator it( &o, KDMetaMethodIterator::Signal );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed(QObject*)" );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed()" );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( QObject::staticMetaObject, KDMetaMethodIterator::Signal );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed(QObject*)" );
//    }
//    {
//        KDMetaMethodIterator it( o.metaObject(), KDMetaMethodIterator::Signal );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed(QObject*)" );
//    }

//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Signal );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed(QObject*)" );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "destroyed()" );
//        assertEqual( std::string( it.connectableSignature() ), "2destroyed()" );
//        assertFalse( it.hasNext() );
//    }

//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Signal, KDMetaMethodIterator::IgnoreQObjectMethods );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Signal, KDMetaMethodIterator::IgnoreSuperClassMethods );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Slot );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "deleteLater()" );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "publicSlot(int)" );
//        assertEqual( std::string( it.connectableSignature() ), "1publicSlot(int)" );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Slot, KDMetaMethodIterator::IgnoreQObjectMethods );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "publicSlot(int)" );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Slot, KDMetaMethodIterator::Protected, KDMetaMethodIterator::IgnoreQObjectMethods );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "protectedSlot(int)" );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClass::staticMetaObject, KDMetaMethodIterator::Slot, KDMetaMethodIterator::Private, KDMetaMethodIterator::IgnoreQObjectMethods );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "privateSlot(int)" );
//        assertEqual( std::string( it.next().signature() ), "veryPrivateSlot()" );
//        assertFalse( it.hasNext() );
//    }

//    {
//        KDMetaMethodIterator it( TestClassDerived::staticMetaObject, KDMetaMethodIterator::Slot, KDMetaMethodIterator::Private, KDMetaMethodIterator::IgnoreQObjectMethods );
//        assertTrue( it.hasNext() );
//        assertEqual( std::string( it.next().signature() ), "privateSlot(int)" );
//        assertEqual( std::string( it.next().signature() ), "veryPrivateSlot()" );
//        assertFalse( it.hasNext() );
//    }
//    {
//        KDMetaMethodIterator it( TestClassDerived::staticMetaObject, KDMetaMethodIterator::Slot, KDMetaMethodIterator::Private, KDMetaMethodIterator::IgnoreSuperClassMethods );
//        assertFalse( it.hasNext() );
//    }

//}

//#include "kdmetamethoditerator.moc"

//#endif // KDTOOLSCORE_UNITTESTS
