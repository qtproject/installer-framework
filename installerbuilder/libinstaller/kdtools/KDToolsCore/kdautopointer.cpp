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

#include "kdautopointer.h"

/*!
  \class KDAutoPointer
  \ingroup core smartptr
  \brief Owning version of QPointer
  \since_c 2.1

  (The exception safety of this class has not been evaluated yet.)

  KDAutoPointer is a fusion of std::auto_ptr and QPointer, ie. it owns the
  contained object, but if that object is otherwise deleted, it sets
  itself to null, just like QPointer does.

  Like std::auto_ptr, ownership is never shared between instances of
  KDAutoPointer. Copying and copy assignment \em transfer ownership,
  ie. in the following code

  \code
  KDAutoPointer<MyDialog> myDialog( new MyDialog );
  KDAutoPointer<QObject> monitor( myDialog ); // Probably WRONG!
  \endcode

  \c monitor will end up owning the \c MyDialog instance, and \c
  myDialog will be NULL, which is probably not what the programmer
  intended.

  Like QPointer, KDAutoPointer can only hold objects of a class derived
  from QObject. If you need this functionality for arbitrary types,
  consider using std::tr1::shared_ptr together with
  std::tr1::weak_ptr.

  \section overview Overview

  You construct a KDAutoPointer by passing a pointer to the object to be
  owned in \link KDAutoPointer(T*) the constructor\endlink, or, after the
  fact, by calling reset(). Neither of these operations happen
  implicitly, since they entail a change of ownership that should be
  visible in code.

  For the same reason, if you need to get a pointer to the contained
  (owned) object, the conversion is not implicit, either. You have to
  explicitly call get() (if you want to leave ownership to the
  KDAutoPointer) or release() (if you want to transfer ownership out of
  KDAutoPointer).

  Use \link swap(KDAutoPointer&,KDAutoPointer&) swap()\endlink to quickly
  exchange the contents of two \link KDAutoPointer KDAutoPointers\endlink, and
  operator unspecified_bool_type() to test whether KDAutoPointer (still)
  contains an object:

  \code
  KDAutoPointer w1( new QWidget ), w2();
  assert( w1 ); assert( !w2 );
  swap( w1, s2 );
  assert( !w1 ); assert( w2 );
  \endcode

  \section general-use General Use

  The general use case of KDAutoPointer is that of holding objects that
  have unclear ownership semantics. E.g. classes that implement the
  Command Pattern are sometimes implemented as deleting themselves
  after the command finished. By holding instances of such classes in
  KDAutoPointers, you do not have to care about whether or now the object
  deletes itself. If it deletes itself, then KDAutoPointer will set itself
  to NULL, if not, KDAutoPointer will delete the object when it goes out
  of scope.

  \section child-creation Esp.: Child Creation

  You can also use KDAutoPointer for temporarily storing \link QObject
  QObjects\endlink for exception safety. This is esp. useful when
  following the Trolltech-recommended style of dialog creation (create
  parent-less children up-front, add to layouts later):

  \code
  QLabel * lb = new QLabel( tr("Name:" );
  QLineEdit * le = new QLineEdit;

  QHBoxLayout * hbox = new QHBoxLayout;
  hbox->addWidget( lb );
  hbox->addWidget( le );
  // if somethings throws an exception here,
  // all of the above will be leaked
  setLayout( hbox );
  \endcode

  Rewriting this as (granted, cumbersome):
  \code
  KDAutoPointer<QLabel> lb( new QLabel( tr("Name:" );
  KDAutoPointer<QLineEdit> le( new QLineEdit );

  KDAutoPointer<QHBoxLayout> hbox( new QHBoxLayout );
  hbox->addWidget( lb.get() );
  hbox->addWidget( le.get() );
  // if somethings throws an exception here,
  // all of the above will be properly cleaned up
  setLayout( hbox.release() );
  lb.release();
  le.release();
  \endcode
  takes care of the leak.

  Since this is so cumbersome, you should instead always pass parents
  to children as part of construction.

  \section model-dialogs Esp.: Modal Dialogs

  A more specific use case is (modal) dialogs.

  Traditionally, most programmers allocate these on the stack:

  \code
  MyDialog dialog( parent );
  // set up 'dialog'...
  if ( !dialog.exec() )
      return;
  // extract information from 'dialog'...
  return;
  \endcode

  This is usually ok. Even though \c dialog has a parent, the parent
  will usually live longer than the dialog. The dialog is modal, after
  all, so the user is not supposed to be able to quit the application
  while that dialog is on screen.

  There are some applications, however, that have other ways to
  terminate than explicit user request. Whether the application exits
  timer-driven, or by an IPC call, what will happen is that \c dialog
  is deleted as a child of \c parent \em and \em then \em again by the
  compiler as part of cleaning up the stack once \c exec() returns.

  This is where KDAutoPointer comes in. Instead of allocating modal
  dialogs on the stack, allocate them into a KDAutoPointer:

  \code
  const KDAutoPointer<MyDialog> dialog( new MyDialog( parent ) );
  // set up 'dialog'...
  if ( !dialog->exec() || !dialog )
      return;
  // extract information from 'dialog'...
  return;
  \endcode

  Note the use of \c const to prevent accidental copying of \c dialog,
  and the additional check for \c dialog to catch the case where after
  exec() returns, the dialog has been deleted.
*/

/*!
  \typedef KDAutoPointer::element_type

  A typedef for \c T. For STL compatibility.
*/

/*!
  \typedef KDAutoPointer::value_type

  A typedef for \c T. For STL compatibility.
*/

/*!
  \typedef KDAutoPointer<T>::pointer

  A typedef for \c T*. For STL compatibility.
*/

/*!
  \fn KDAutoPointer::KDAutoPointer()

  Default constructor. Constructs a NULL KDAutoPointer.

  \post get() == 0
*/

/*!
  \fn KDAutoPointer::KDAutoPointer( T * obj )

  Constructor. Constructs a KDAutoPointer that contains (owns) \a obj.

  \post get() == obj
*/

/*!
  \fn KDAutoPointer::KDAutoPointer( KDAutoPointer & other )

  Move constructor. Constructs a KDAutoPointer that takes over ownership
  of the object contained (owned) by \a other.

  \post this->get() == (the object previously owned by 'other')
  \post other.get() == 0
*/

/*!
  \fn KDAutoPointer::KDAutoPointer( KDAutoPointer<U> & other )

  \overload
*/

/*!
  \fn KDAutoPointer & KDAutoPointer::operator=( KDAutoPointer & other )

  Copy assignment operator. Equivalent to
  \code
  reset( other.release() );
  \endcode

  \post this->get() == (the object previously owned by 'other')
  \post other.get() == 0
  \post The object previously owned by \c *this has been deleted.
*/

/*!
  \fn KDAutoPointer & KDAutoPointer::operator=( KDAutoPointer<U> & other )

  \overload
*/

/*!
  \fn KDAutoPointer::~KDAutoPointer()

  Destructor.

  \post The object previously owned by \c *this has been deleted.
*/

/*!
  \fn void KDAutoPointer::swap( KDAutoPointer & other )

  Swaps the contents of \a *this and \a other. You probably want to
  use the free function swap(KDAutoPointer&,KDAutoPointer&) instead. Also
  qSwap() has been specialized to use this function internally.

  \post get() == (previous value of other.get())
  \post other.get() == (previous value of this->get())
*/

/*!
  \fn T * KDAutoPointer::get() const

  \returns a pointer to the contained (owned) object.
*/

/*!
  \fn T * KDAutoPointer::release()

  Yields ownership of the contained object and returns a pointer to it.

  \post get() == 0
  \post The object previously owned by \c *this had \em not been deleted.
*/

/*!
  \fn T * KDAutoPointer::data() const

  Equivalent to get(). Provided for consistency with Qt naming
  conventions.
*/

/*!
  \fn T * KDAutoPointer::take()

  Equivalent to release(). Provided for consistency with Qt naming
  conventions.
*/

/*!
  \fn void KDAutoPointer::reset( T * other )

  Yields ownership of the contained object (if any) and and takes over
  ownership of \a other.

  \post get() == other
  \post The object previously owned by \c *this has been deleted.
*/

/*!
  \fn T & KDAutoPointer::operator*() const

  Dereference operator. Returns \link get() *get()\endlink.
*/

/*!
  \fn T * KDAutoPointer::operator->() const

  Member-by-pointer operator. Returns get().
*/

/*!
  \fn bool KDAutoPointer::operator==( const KDAutoPointer<S> & other ) const

  Equal-to operator. Returns get() == other.get().

  \note Due to the fact that two \link KDAutoPointer KDAutoPointers\endlink
  cannot contain the same object, the relational operators are not
  very useful.
*/

/*!
  \fn bool KDAutoPointer::operator!=( const KDAutoPointer<S> & other ) const

  Not-Equal-to operator. Returns get() != other.get().

  \note Due to the fact that two \link KDAutoPointer KDAutoPointers\endlink
  cannot contain the same object, the relational operators are not
  very useful.
*/

/*!
  \fn KDAutoPointer::operator unspecified_bool_type() const

  \returns \c true if \c *this contains an object, otherwise \c false.
*/

/*!
  \fn void swap( KDAutoPointer<T> & lhs, KDAutoPointer<T> & rhs )
  \relates KDAutoPointer

  Equivalent to
  \code
  lhs.swap( rhs );
  \endcode

  Provided for generic code that uses the standard swap idiom:
  \code
  using std::swap;
  swap( a, b );
  \endcode
  to find through <a href="http://en.wikipedia.org/wiki/Argument_dependent_name_lookup">ADL</a>,
  enabling such code to automatically use the more efficient
  \link KDAutoPointer::swap() member swap\endlink instead of the default
  implementation (which uses a temporary).
*/

/*!
  \fn void qSwap( KDAutoPointer<T> & lhs, KDAutoPointer<S> & rhs )
  \relates KDAutoPointer

  Equivalent to
  \code
  swap( lhs, rhs );
  \endcode
  except for code using %qSwap() instead of (std::)swap.
*/

/*!
  \fn bool operator==( const KDAutoPointer<S> & lhs, const T * rhs )
  \relates KDAutoPointer

  Equal-to operator. Equivalent to
  \code
  lhs.get() == rhs
  \endcode
*/

/*!
  \fn bool operator==( const S * lhs, const KDAutoPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/

/*!
  \fn bool operator==( const KDAutoPointer<S> & lhs, const QPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/

/*!
  \fn bool operator==( const QPointer<S> & lhs, const KDAutoPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/


/*!
  \fn bool operator!=( const KDAutoPointer<S> & lhs, const T * rhs )
  \relates KDAutoPointer

  Not-equal-to operator. Equivalent to
  \code
  lhs.get() != rhs
  \endcode
*/

/*!
  \fn bool operator!=( const S * lhs, const KDAutoPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/

/*!
  \fn bool operator!=( const KDAutoPointer<S> & lhs, const QPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/

/*!
  \fn bool operator!=( const QPointer<S> & lhs, const KDAutoPointer<T> & rhs )
  \relates KDAutoPointer
  \overload
*/

#ifdef KDTOOLSCORE_UNITTESTS

#include <KDUnitTest/Test>

namespace {

    struct A  : QObject {};
    struct B  : QObject {};

}

KDAB_UNITTEST_SIMPLE( KDAutoPointer, "kdcoretools" ) {

    {
        QPointer<QObject> o;
        {
            const KDAutoPointer<QObject> qobject( new QObject );
            o = qobject.get();
        }
        assertNull( o );
    }

    {
        // check that conversion to bool works
        KDAutoPointer<A> a( new A );
        const bool b1 = a;
        assertTrue( b1 );
        const bool b2 = !a;
        assertFalse( b2 );
        a.reset();
        const bool b3 = a;
        assertFalse( b3 );
        const bool b4 = !a;
        assertTrue( b4 );
    }

    {
        KDAutoPointer<QObject> o;
        assertNull( o.get() );
        assertFalse( o );
        QObject * obj;
        o.reset( obj = new QObject );
        assertNotNull( o.get() );
        assertTrue( o );
        assertEqual( o.get(), obj );

        KDAutoPointer<QObject> o2 = o;
        assertNull( o.get() );
        assertNotNull( o2.get() );
        assertEqual( o2.get(), obj );

        delete obj;
        assertNull( o2.get() );
    }

    {
        QPointer<QObject> obj = new QObject;
        KDAutoPointer<QObject> o( obj );
        o = o; // self-assignment
        assertNotNull( obj );
        o.reset( o.get() ); // self-assignment-ish
        assertNotNull( obj );

        o.reset( new QObject );
        assertNull( obj );
        assertNotNull( o.get() );
    }

    {
        QPointer<A> ap;
        {
            KDAutoPointer<A> a( new A );
            ap = a.get();
            KDAutoPointer<QObject> o1( a );
            assertNull( a.get() );
            assertNotNull( o1.get() );
            KDAutoPointer<QObject> o2;
            o2 = a;
            // doesn't compile (but doesn't compile for std::auto_ptr,
            // either, so it's ok, I think
            //KDAutoPointer<QObject> o3 = a
        }
        assertNull( ap );
    }

    {
        // test swapping
        KDAutoPointer<A> a1( new A ), a2( new A );

        A * A1 = a1.get();
        A * A2 = a2.get();

        using std::swap;
        swap( a1, a2 );

        assertNotNull( a1.get() );
        assertNotNull( a2.get() );

        assertEqual( a1.get(), A2 );
        assertEqual( a2.get(), A1 );

        qSwap( a1, a2 );
        assertEqual( a1.get(), A1 );
        assertEqual( a2.get(), A2 );

    }

    {
        KDAutoPointer<A> apa( new A );
        QPointer<A>  qpa( apa.get() );
        A * pa = apa.get();
        const A * cpa = pa;

        assertEqual( apa.get(), apa.get() );
        assertEqual( apa.get(), qpa );
        assertEqual( apa.get(),  pa );
        assertEqual( apa.get(), cpa );
        assertEqual( qpa, apa.get() );
        assertEqual(  pa, apa.get() );
        assertEqual( cpa, apa.get() );

        KDAutoPointer<QObject> apq; cpa = pa = qpa = 0;

        assertNotEqual( apa.get(), qpa );
        assertNotEqual( apa.get(),  pa );
        assertNotEqual( apa.get(), cpa );
        assertNotEqual( qpa, apa.get() );
        assertNotEqual(  pa, apa.get() );
        assertNotEqual( cpa, apa.get() );

        assertNotEqual( apq.get(), apa.get() );
        assertNotEqual( apa.get(), apq.get() );

        assertNotNull( apa.get() );

        apq.reset( new QObject );

        assertNotEqual( apa.get(), qpa );
        assertNotEqual( apa.get(),  pa );
        assertNotEqual( apa.get(), cpa );
        assertNotEqual( qpa, apa.get() );
        assertNotEqual(  pa, apa.get() );
        assertNotEqual( cpa, apa.get() );

        assertNotEqual( apq.get(), apa.get() );
        assertNotEqual( apa.get(), apq.get() );

        assertNotNull( apa.get() );
    }
}

#endif // KDTOOLSCORE_UNITTESTS
