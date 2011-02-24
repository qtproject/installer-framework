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

#ifndef KDUPDATERSIGNATUREVERIFICATIONJOB_H
#define KDUPDATERSIGNATUREVERIFICATIONJOB_H

#include <KDToolsCore/pimpl_ptr.h>
#include <QtCore/QGenericArgument>
#include <QtCore/QRunnable>

QT_BEGIN_NAMESPACE
class QByteArray;
class QIODevice;
class QObject;
template <typename T> class QVector;
QT_END_NAMESPACE

namespace KDUpdater {
    class SignatureVerifier;
    class SignatureVerificationResult;

    class Runnable : public QRunnable {
    public:
        Runnable();
        ~Runnable();

        void addResultListener( QObject* receiver, const char* method );

    protected:
        void emitResult( const QGenericArgument& arg0=QGenericArgument( 0 ),
                         const QGenericArgument& arg1=QGenericArgument(),
                         const QGenericArgument& arg2=QGenericArgument(),
                         const QGenericArgument& arg3=QGenericArgument(),
                         const QGenericArgument& arg4=QGenericArgument(),
                         const QGenericArgument& arg5=QGenericArgument(),
                         const QGenericArgument& arg6=QGenericArgument(),
                         const QGenericArgument& arg7=QGenericArgument(),
                         const QGenericArgument& arg8=QGenericArgument(),
                         const QGenericArgument& arg9=QGenericArgument() );

    private:
        class Private;
        kdtools::pimpl_ptr<Private> d;
    };

    class SignatureVerificationRunnable : public Runnable {
    public:
        explicit SignatureVerificationRunnable();
        ~SignatureVerificationRunnable();

        const SignatureVerifier* verifier() const;
        void setVerifier( const SignatureVerifier* verifier );

        QByteArray signature() const;
        void setSignature( const QByteArray& sig );

        QIODevice* data() const;
        void setData( QIODevice* device );

        /* reimp */ void run();

    private:
        class Private;
        kdtools::pimpl_ptr<Private> d;
    };
}

#endif // KDUPDATERSIGNATUREVERIFICATIONJOB_H
