/**************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "operationrunner.h"

#include <errors.h>
#include <updateoperationfactory.h>
#include <packagemanagercore.h>

#include <QMetaObject>

#include <iostream>

OperationRunner::OperationRunner(qint64 magicMarker,
        const QList<QInstaller::OperationBlob> &oldOperations)
    : m_core(new QInstaller::PackageManagerCore(magicMarker, oldOperations))
{
    // We need a package manager core as some operations expect them to be available.
}

OperationRunner::~OperationRunner()
{
    delete m_core;
}

int OperationRunner::runOperation(QStringList arguments, RunMode mode)
{
    int result = EXIT_FAILURE;
    try {
        const QString name = arguments.takeFirst();
        QScopedPointer<QInstaller::Operation> op(KDUpdater::UpdateOperationFactory::instance()
            .create(name, m_core));
        if (!op) {
            std::cerr << "Cannot instantiate operation: " << qPrintable(name) << std::endl;
            return EXIT_FAILURE;
        }

        if (QObject *const object = dynamic_cast<QObject*> (op.data())) {
            const QMetaObject *const mo = object->metaObject();
            if (mo->indexOfSignal(mo->normalizedSignature("outputTextChanged(QString)")) > -1)
                connect(object, SIGNAL(outputTextChanged(QString)), this, SLOT(print(QString)));
        }
        op->setArguments(arguments);

        bool readyPerformed = false;
        if (mode == RunMode::Do)
            readyPerformed = op->performOperation();
        if (mode == RunMode::Undo)
            readyPerformed = op->undoOperation();

        if (readyPerformed) {
            result = EXIT_SUCCESS;
            std::cout << "Operation finished successfully." << std::endl;
        } else {
            std::cerr << "An error occurred while performing the operation: "
                << qPrintable(op->errorString()) << std::endl;
        }
    } catch (const QInstaller::Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
    } catch (...) {
        std::cerr << "Unknown exception caught." << std::endl;
    }
    return result;
}

void OperationRunner::print(const QString &value)
{
    std::cout << qPrintable(value) << std::endl;
}
