/**************************************************************************
**
** This file is part of Installer Framework
**
** Copyright (c) 2011-2012 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
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
** rights. These rights are described in the Nokia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** Other Usage
**
** Alternatively, this file may be used in accordance with the terms and
** conditions contained in a signed written agreement between you and Nokia.
**
** If you have questions regarding the use of this file, please contact
** Nokia at qt-info@nokia.com.
**
**************************************************************************/
#include "operationrunner.h"

#include "binaryformat.h"
#include "component.h"
#include "errors.h"
#include "init.h"
#include "packagemanagercore.h"
#include "utils.h"

#include "kdupdaterupdateoperation.h"
#include "kdupdaterupdateoperationfactory.h"

#include <iostream>

namespace {
class OutputHandler : public QObject
{
    Q_OBJECT

public slots:
   void drawItToCommandLine(const QString &outPut)
   {
       std::cout << qPrintable(outPut) << std::endl;
   }
};
}

using namespace QInstaller;
using namespace QInstallerCreator;

OperationRunner::OperationRunner()
    : m_core(0)
{
    QInstaller::init();
}

OperationRunner::~OperationRunner()
{
    delete m_core;
}

bool OperationRunner::init()
{
    try {
        BinaryContent content = BinaryContent::readAndRegisterFromApplicationFile();
        m_core = new PackageManagerCore(content.magicMarker(), content.performedOperations());
    } catch (const Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
        return false;
    } catch (...) {
        return false;
    }

    return true;
}

void OperationRunner::setVerbose(bool verbose)
{
    QInstaller::setVerbose(verbose);
}

int OperationRunner::runOperation(const QStringList &arguments)
{
    if (!init()) {
        qDebug() << "Could not init the package manager core - without this not all operations are working "
            "as expected.";
    }

    bool isPerformType = arguments.contains(QLatin1String("--runoperation"));
    bool isUndoType = arguments.contains(QLatin1String("--undooperation"));

    if ((!isPerformType && !isUndoType) || (isPerformType && isUndoType)) {
        std::cerr << "wrong arguments are used, cannot run this operation";
        return EXIT_FAILURE;
    }

    QStringList argumentList;

    if (isPerformType)
        argumentList = arguments.mid(arguments.indexOf(QLatin1String("--runoperation")) + 1);
    else
        argumentList = arguments.mid(arguments.indexOf(QLatin1String("--undooperation")) + 1);


    try {
        const QString operationName = argumentList.takeFirst();
        KDUpdater::UpdateOperation* const operation = KDUpdater::UpdateOperationFactory::instance()
            .create(operationName);
        if (!operation) {
            std::cerr << "Can not find the operation: " << qPrintable(operationName) << std::endl;
            return EXIT_FAILURE;
        }

        OutputHandler myOutPutHandler;
        QObject *const operationObject = dynamic_cast<QObject *>(operation);
        if (operationObject != 0) {
            const QMetaObject *const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
                QObject::connect(operationObject, SIGNAL(outputTextChanged(QString)),
                        &myOutPutHandler, SLOT(drawItToCommandLine(QString)));
            }
        }

        operation->setValue(QLatin1String("installer"), QVariant::fromValue(m_core));

        operation->setArguments(argumentList);

        bool readyPerformed = false;
        if (isPerformType)
            readyPerformed = operation->performOperation();

        if (isUndoType)
            readyPerformed = operation->undoOperation();

        std::cout << "========================================" << std::endl;
        if (readyPerformed) {
            std::cout << "Operation was successfully performed." << std::endl;
        } else {
            std::cerr << "There was a problem while performing the operation: "
                << qPrintable(operation->errorString()) << std::endl;
            return EXIT_FAILURE;
        }
    } catch (const QInstaller::Error &e) {
        std::cerr << qPrintable(e.message()) << std::endl;
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "Caught unknown exception" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

#include "operationrunner.moc"
