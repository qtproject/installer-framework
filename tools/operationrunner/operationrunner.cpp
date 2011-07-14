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
#include <packagemanagercore.h>

#include <common/errors.h>
#include <common/utils.h>
#include <common/repositorygen.h>

#include <init.h>
#include <KDUpdater/UpdateOperation>
#include <KDUpdater/UpdateOperationFactory>

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QDir>

#include <iostream>

static void printUsage()
{
    std::cout << "Usage: " << std::endl;
    std::cout << std::endl;
    std::cout << "operationrunner \"Execute\" \"{0,1}\" \"C:\\Windows\\System32\\cmd.exe\" \"/A\" \"/Q\" \"/C\" \"magicmaemoscript.bat\" \"showStandardError\"" << std::endl;
    std::cout << std::endl;
    std::cout << std::endl;
    std::cout << "Note: there is an optional argument --sdktargetdir which is needed by some operations" << std::endl;
    std::cout << std::endl;
    std::cout << "operationrunner --sdktargetdir c:\\QtSDK \"RegisterToolChain\" \"GccToolChain\" \"Qt4ProjectManager.ToolChain.GCCE\" \"GCCE 4 for Symbian targets\" \"arm-symbian-device-elf-32bit\" \"c:\\QtSDK\\Symbian\\tools\\gcce4\\bin\\arm-none-symbianelf-g++.exe\""<< std::endl;
}

class OutputHandler : public QObject
{
    Q_OBJECT

public slots:
   void drawItToCommandLine(const QString &outPut)
   {
       std::cout << qPrintable(outPut) << std::endl;
   }
};



int main(int argc, char **argv)
{
    try {
        QCoreApplication app(argc, argv);

        QStringList argumentList = app.arguments();

        if( argumentList.count() < 2 || argumentList.contains("--help") )
        {
            printUsage();
            return 1;
        }
        argumentList.removeFirst(); // we don't need the application name

        QString sdkTargetDir;
        int sdkTargetDirArgumentPosition = argumentList.indexOf(QRegExp("--sdktargetdir", Qt::CaseInsensitive));
        //+1 means the needed following argument
        if (sdkTargetDirArgumentPosition != -1 && argumentList.count() > sdkTargetDirArgumentPosition + 1) {
            sdkTargetDir = argumentList.at(sdkTargetDirArgumentPosition + 1);
            if (!QDir(sdkTargetDir).exists()) {
                std::cerr << qPrintable(QString("The following argument of %1 is not an existing directory.").arg(
                    argumentList.at(sdkTargetDirArgumentPosition))) << std::endl;
                return 1;
            }
            argumentList.removeAt(sdkTargetDirArgumentPosition + 1);
            argumentList.removeAt(sdkTargetDirArgumentPosition);
        }


        QInstaller::init();

        QInstaller::VerboseWriter::instance();

        QInstaller::setVerbose( true );


        QString operationName = argumentList.takeFirst();
        KDUpdater::UpdateOperation* const operation = KDUpdater::UpdateOperationFactory::instance().create(operationName);
        if (!operation) {
            std::cerr << "Can not find the operation: " << qPrintable(operationName) << std::endl;
            return 1;
        }

        OutputHandler myOutPutHandler;
        QObject* const operationObject = dynamic_cast<QObject*>(operation);
        if (operationObject != 0) {
            const QMetaObject* const mo = operationObject->metaObject();
            if (mo->indexOfSignal(QMetaObject::normalizedSignature("outputTextChanged(QString)")) > -1) {
                QObject::connect(operationObject, SIGNAL(outputTextChanged(QString)),
                        &myOutPutHandler, SLOT(drawItToCommandLine(QString)));
            }
        }

        QInstaller::PackageManagerCore packageManagerCore;
        if (!sdkTargetDir.isEmpty()) {
            packageManagerCore.setValue(QLatin1String("TargetDir"), QFileInfo(sdkTargetDir).absoluteFilePath());

            operation->setValue(QLatin1String("installer"),
                QVariant::fromValue(&packageManagerCore));
        }

        operation->setArguments(argumentList);

        bool readyPerformed = operation->performOperation();
        std::cout << "========================================" << std::endl;
        if (readyPerformed) {
            std::cout << "Operation was succesfully performed." << std::endl;
        } else {
            std::cerr << "There was a problem while performing the operation: " << qPrintable(operation->errorString()) << std::endl;
            std::cerr << "\tNote: if you see something like installer is null/empty then --sdktargetdir argument was missing." << std::endl;
        }
        return 0;
    } catch ( const QInstaller::Error& e ) {
        std::cerr << qPrintable(e.message()) << std::endl;
    }
    return 1;
}

#include "operationrunner.moc"
