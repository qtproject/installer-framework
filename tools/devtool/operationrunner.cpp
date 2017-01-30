/**************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
