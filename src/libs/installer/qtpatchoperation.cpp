/**************************************************************************
**
** Copyright (C) 2012-2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
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
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "qtpatchoperation.h"
#include "qtpatch.h"
#ifdef Q_OS_OSX
#include "macreplaceinstallnamesoperation.h"
#endif

#include "packagemanagercore.h"

#include <QSet>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDirIterator>
#include <QtCore/QDebug>

using namespace QInstaller;

static QHash<QByteArray, QByteArray> generatePatchValueHash(const QByteArray &newQtPath,
        const QHash<QString, QByteArray> &qmakeValueHash, const QString &type)
{
    QHash<QByteArray, QByteArray> replaceHash; //first == searchstring: second == replace string
    char nativeSeperator = QDir::separator().toLatin1();
    QByteArray oldValue;

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_PREFIX"));
    replaceHash.insert(QByteArray("qt_prfxpath=%1").replace("%1", oldValue),
        QByteArray("qt_prfxpath=%1/").replace("%1/", newQtPath));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_ARCHDATA"));
    replaceHash.insert(QByteArray("qt_adatpath=%1").replace("%1", oldValue),
        QByteArray("qt_adatpath=%1/").replace("%1/", newQtPath));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_DOCS"));
    replaceHash.insert(QByteArray("qt_docspath=%1").replace("%1", oldValue),
        QByteArray("qt_docspath=%1/doc").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_HEADERS"));
    replaceHash.insert(QByteArray("qt_hdrspath=%1").replace("%1", oldValue),
        QByteArray("qt_hdrspath=%1/include").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_LIBS"));
    replaceHash.insert(QByteArray("qt_libspath=%1").replace("%1", oldValue),
        QByteArray("qt_libspath=%1/lib").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_LIBEXECS"));
    if (type == QLatin1String("windows")) {
        replaceHash.insert(QByteArray("qt_lbexpath=%1").replace("%1", oldValue),
                           QByteArray("qt_lbexpath=%1/bin").replace("%1/",
                                                                    newQtPath + nativeSeperator));
    } else {
        replaceHash.insert(QByteArray("qt_lbexpath=%1").replace("%1", oldValue),
                           QByteArray("qt_lbexpath=%1/libexec").replace("%1/",
                                                                        newQtPath + nativeSeperator));
    }

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_BINS"));
    replaceHash.insert(QByteArray("qt_binspath=%1").replace("%1", oldValue),
        QByteArray("qt_binspath=%1/bin").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_PLUGINS"));
    replaceHash.insert(QByteArray("qt_plugpath=%1").replace("%1", oldValue),
        QByteArray("qt_plugpath=%1/plugins").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_IMPORTS"));
    replaceHash.insert(QByteArray("qt_impspath=%1").replace("%1", oldValue),
        QByteArray("qt_impspath=%1/imports").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_QML"));
    replaceHash.insert(QByteArray("qt_qml2path=%1").replace("%1", oldValue),
        QByteArray("qt_qml2path=%1/qml").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_DATA"));
    replaceHash.insert( QByteArray("qt_datapath=%1").replace("%1", oldValue),
        QByteArray("qt_datapath=%1/").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_TRANSLATIONS"));
    replaceHash.insert( QByteArray("qt_trnspath=%1").replace("%1", oldValue),
        QByteArray("qt_trnspath=%1/translations").replace("%1/", newQtPath + nativeSeperator));

    // This must not be patched!
    // On desktop there should be a correct default path (for example "/etc/xdg"),
    // but on some other targets you need to use "-sysconfdir </your/default/config/path"
    // while building Qt to get a correct QT_INSTALL_CONFIGURATION value
    //        oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_CONFIGURATION"));
    //        replaceMap.insert( QByteArray("qt_stngpath=%1").replace("%1", oldValue),
    //                            QByteArray("qt_stngpath=%1").replace("%1", newQtPath));

    //examples and demos can patched outside separately,
    //but for cosmetic reasons - if the qt version gets no examples later.
    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_EXAMPLES"));
    replaceHash.insert( QByteArray("qt_xmplpath=%1").replace("%1", oldValue),
        QByteArray("qt_xmplpath=%1/examples").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_DEMOS"));
    replaceHash.insert( QByteArray("qt_demopath=%1").replace("%1", oldValue),
        QByteArray("qt_demopath=%1/demos").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_INSTALL_TESTS"));
    replaceHash.insert(QByteArray("qt_tstspath=%1").replace("%1", oldValue),
        QByteArray("qt_tstspath=%1/tests").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_HOST_PREFIX"));
    replaceHash.insert(QByteArray("qt_hpfxpath=%1").replace("%1", oldValue),
        QByteArray("qt_hpfxpath=%1/").replace("%1/", newQtPath));

    oldValue = qmakeValueHash.value(QLatin1String("QT_HOST_BINS"));
    replaceHash.insert( QByteArray("qt_hbinpath=%1").replace("%1", oldValue),
        QByteArray("qt_hbinpath=%1/bin").replace("%1/", newQtPath + nativeSeperator));

    oldValue = qmakeValueHash.value(QLatin1String("QT_HOST_DATA"));
    replaceHash.insert(QByteArray("qt_hdatpath=%1").replace("%1", oldValue),
        QByteArray("qt_hdatpath=%1/").replace("%1/", newQtPath));

    oldValue = qmakeValueHash.value(QLatin1String("QT_HOST_LIBS"));
    replaceHash.insert(QByteArray("qt_hlibpath=%1").replace("%1", oldValue),
        QByteArray("qt_hlibpath=%1/lib").replace("%1/", newQtPath + nativeSeperator));

    return replaceHash;
}

QtPatchOperation::QtPatchOperation()
{
    setName(QLatin1String("QtPatch"));
}

void QtPatchOperation::backup()
{
}

bool QtPatchOperation::performOperation()
{
    // Arguments:
    // 1. type
    // 2. new/target qtpath
    // 3. version if greather Qt4
    // optional QmakeOutputInstallerKey=<used_installer_value>

    // the possible 2 argument case is here to support old syntax
    if (arguments().count() < 2 && arguments().count() > 4) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("3 or 4"), QLatin1String("")));
        return false;
    }

    QStringList args = arguments();
    QString qmakeOutputInstallerKey;
    QStringList filteredQmakeOutputInstallerKey = args.filter(QLatin1String("QmakeOutputInstallerKey="),
        Qt::CaseInsensitive);
    PackageManagerCore *const core = value(QLatin1String("installer")).value<PackageManagerCore*>();
    if (!filteredQmakeOutputInstallerKey.isEmpty()) {
        if (!core) {
            setError(UserDefinedError);
            setErrorString(tr("Needed installer object in \"%1\" operation is empty.").arg(name()));
            return false;
        }
        QString qmakeOutputInstallerKeyArgument = filteredQmakeOutputInstallerKey.at(0);
        qmakeOutputInstallerKey = qmakeOutputInstallerKeyArgument;
        qmakeOutputInstallerKey.replace(QLatin1String("QmakeOutputInstallerKey="), QString(), Qt::CaseInsensitive);
        args.removeAll(qmakeOutputInstallerKeyArgument);
    }

    QString type = args.at(0);
    bool isPlatformSupported = type.contains(QLatin1String("linux"), Qt::CaseInsensitive)
        || type.contains(QLatin1String("windows"), Qt::CaseInsensitive)
        || type.contains(QLatin1String("mac"), Qt::CaseInsensitive);
    if (!isPlatformSupported) {
        setError(InvalidArguments);
        setErrorString(tr("First argument should be 'linux', 'mac' or 'windows'. No other type is supported "
            "at this time."));
        return false;
    }

    if (core && !filteredQmakeOutputInstallerKey.isEmpty() && core->value(qmakeOutputInstallerKey).isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("Could not find the needed QmakeOutputInstallerKey(%1) value on the installer "
            "object. The ConsumeOutput operation on the valid qmake needs to be called first.").arg(
            qmakeOutputInstallerKey));
        return false;
    }

    const QString newQtPathStr = QDir::toNativeSeparators(args.at(1));
    const QByteArray newQtPath = newQtPathStr.toUtf8();
    QString qmakePath = QString::fromUtf8(newQtPath) + QLatin1String("/bin/qmake");
#ifdef Q_OS_WIN
    qmakePath = qmakePath + QLatin1String(".exe");
#endif

    QHash<QString, QByteArray> qmakeValueHash;
    if (core && !core->value(qmakeOutputInstallerKey).isEmpty()) {
        qmakeValueHash = QtPatch::readQmakeOutput(core->value(qmakeOutputInstallerKey).toLatin1());
    } else {
        if (!QFile::exists(qmakePath)) {
            setError(UserDefinedError);
            setErrorString(tr("QMake from the current Qt version \n(%1)is not existing. Please file a bugreport "
                "with this dialog at https://bugreports.qt-project.org.").arg(QDir::toNativeSeparators(qmakePath)));
            return false;
        }
        QByteArray qmakeOutput;
        qmakeValueHash = QtPatch::qmakeValues(qmakePath, &qmakeOutput);
        if (qmakeValueHash.isEmpty()) {
            setError(UserDefinedError);
            setErrorString(tr("The output of \n%1 -query\nis not parseable. Please file a bugreport with this "
                "dialog https://bugreports.qt-project.org.\noutput: \"%2\"").arg(QDir::toNativeSeparators(qmakePath),
                QString::fromUtf8(qmakeOutput)));
            return false;
        }
    }


    const QByteArray oldQtPath = qmakeValueHash.value(QLatin1String("QT_INSTALL_PREFIX"));
    bool oldQtPathFromQMakeIsEmpty = oldQtPath.isEmpty();

    //maybe we don't need this, but I 255 should be a rational limit
    if (255 < newQtPath.size()) {
        setError(UserDefinedError);
        setErrorString(tr("Qt patch error: new Qt dir(%1)\nneeds to be less than 255 characters.")
            .arg(newQtPathStr));
        return false;
    }

#ifdef Q_OS_OSX
    // looking for /lib/Qt wasn't enough for all libs and frameworks,
    // at the Qt4 case we had for example: /lib/libQtCLucene* and /lib/phonon*
    // so now we find every possible replace string inside dynlib dependencies
    // and we reduce it to few as possible search strings
    QStringList possibleSearchStringList;
    QDirIterator dirIterator(newQtPathStr + QLatin1String("/lib/"));
    while (dirIterator.hasNext()) {
        const QString possibleSearchString = QString(dirIterator.next()).remove(newQtPathStr);
        const QFileInfo fileInfo = dirIterator.fileInfo();
        if (fileInfo.isSymLink())
            continue;
        if (fileInfo.isDir()) {
            if (possibleSearchString.endsWith(QLatin1String(".framework")))
                possibleSearchStringList.append(possibleSearchString);
            else
                continue;
        }
        if (possibleSearchString.endsWith(QLatin1String(".dylib")))
            possibleSearchStringList.append(possibleSearchString);
    }

    // now we have this in possibleSearchStringList at Qt 4.8.6
//    "/lib/libQtCLucene.4.8.6.dylib"
//    "/lib/libQtCLucene_debug.4.8.6.dylib"
//    "/lib/phonon.framework"
//    "/lib/QtCore.framework"
//    "/lib/QtDeclarative.framework"
//    "/lib/QtDesigner.framework"
//    "/lib/QtDesignerComponents.framework"
//    "/lib/QtGui.framework"
//    "/lib/QtHelp.framework"
//    "/lib/QtMultimedia.framework"
//    "/lib/QtNetwork.framework"
//    "/lib/QtOpenGL.framework"
//    "/lib/QtScript.framework"
//    "/lib/QtScriptTools.framework"
//    "/lib/QtSql.framework"
//    "/lib/QtSvg.framework"
//    "/lib/QtTest.framework"
//    "/lib/QtWebKit.framework"
//    "/lib/QtXml.framework"
//    "/lib/QtXmlPatterns.framework"

    // so then we reduce the possible filter strings as much as possible
    QStringList searchStringList;

    // as the minimal search string use the subdirector lib + one letter from the name
    int minFilterLength = QString(QLatin1String("/lib/")).length() + 1;

    while (!possibleSearchStringList.isEmpty()) {
        QString firstSearchString = possibleSearchStringList.first();
        int filterLength = minFilterLength;
        int lastFilterCount = 0;
        QString lastFilterString;
        // now filter as long as we find something more then 1
        for (; filterLength < firstSearchString.length(); ++filterLength) {
            QString filterString(firstSearchString.left(filterLength));
            QStringList filteredStringList(possibleSearchStringList.filter(filterString));
            // found a valid filter
            if (lastFilterCount > filteredStringList.count()) {
                possibleSearchStringList = QList<QString>::fromSet(possibleSearchStringList.toSet() -
                    possibleSearchStringList.filter(lastFilterString).toSet());
                searchStringList.append(lastFilterString);
                break;
            } else if (lastFilterCount == 1){ //in case there is only one we can use the complete name
                possibleSearchStringList = QList<QString>::fromSet(possibleSearchStringList.toSet() -
                    possibleSearchStringList.filter(firstSearchString).toSet());
                searchStringList.append(firstSearchString);
                break;
            } else {
                lastFilterCount = possibleSearchStringList.filter(filterString).count();
                lastFilterString = filterString;
            }
        }
    }

    // in the tested Qt 4.8.6 case we have searchStringList ("/lib/libQtCLucene", "/lib/Qt", "/lib/phonon")
    foreach (const QString &searchString, searchStringList) {
        MacReplaceInstallNamesOperation operation;
        operation.setArguments(QStringList()
                               //can not use the old path which is wrong in the webkit case
                               //<< QString::fromUtf8(oldQtPath)
                               << searchString
                               << newQtPathStr + searchString //replace string
                               << newQtPathStr //where
                               );
        if (!operation.performOperation()) {
            setError(operation.error());
            setErrorString(operation.errorString());
            return false;
        }
    }
#endif

    QString fileName;
    if (type == QLatin1String("windows"))
        fileName = QString::fromLatin1(":/files-to-patch-windows");
    else if (type == QLatin1String("linux"))
        fileName = QString::fromLatin1(":/files-to-patch-linux");
    else if (type == QLatin1String("mac"))
        fileName = QString::fromLatin1(":/files-to-patch-macx");

    QFile patchFileListFile(fileName);
    QString version = args.value(2).toLower();
    if (!version.isEmpty())
        patchFileListFile.setFileName(fileName + QLatin1Char('-') + version);

    if (!patchFileListFile.open(QFile::ReadOnly)) {
        setError(UserDefinedError);
        setErrorString(tr("Qt patch error: Can not open %1.(%2)").arg(patchFileListFile.fileName(),
            patchFileListFile.errorString()));
        return false;
    }

    QStringList filters;
    QStringList filesToPatch, textFilesToPatch;
    bool readingTextFilesToPatch = false;

    // read the input file
    QTextStream in(&patchFileListFile);

    forever {
        const QString line = in.readLine();

        if (line.isNull())
            break;

        else if (line.isEmpty())
            continue;

        else if (line.startsWith(QLatin1String("%%")))
            readingTextFilesToPatch = true;

        //with empty old path we don't know what we want to replace
        else if (readingTextFilesToPatch && !oldQtPathFromQMakeIsEmpty) {
            // check if file mask filter
            if (line.startsWith(QLatin1String("*."), Qt::CaseInsensitive)) {
                filters << line;
            }
            textFilesToPatch.append(line);
        }
        else
            filesToPatch.append(line);
    }

    QString prefix = QFile::decodeName(newQtPath);

    if (! prefix.endsWith(QLatin1Char('/')))
        prefix += QLatin1Char('/');

//BEGIN - patch binary files
    QHash<QByteArray, QByteArray> patchValueHash = generatePatchValueHash(newQtPath, qmakeValueHash, type);

    foreach (QString fileName, filesToPatch) {
        fileName.prepend(prefix);
        QFile file(fileName);

        //without a file we can't do anything
        if (!file.exists()) {
            continue;
        }

        if (!QtPatch::openFileForPatching(&file)) {
            setError(UserDefinedError);
            setErrorString(tr("Qt patch error: Can not open %1.(%2)").arg(file.fileName())
                .arg(file.errorString()));
            return false;
        }

        QHashIterator<QByteArray, QByteArray> it(patchValueHash);
        while (it.hasNext()) {
            it.next();
            bool isPatched = QtPatch::patchBinaryFile(&file, it.key(), it.value());
            if (!isPatched) {
                qDebug() << QString::fromLatin1("qpatch: warning: file '%1' could not patched").arg(fileName);
            }
        }
    } //foreach (QString fileName, filesToPatch)
//END - patch binary files

//BEGIN - patch text files
    QByteArray newQtPathWithNormalSlashes = QDir::fromNativeSeparators(newQtPathStr).toUtf8();

    QHash<QByteArray, QByteArray> searchReplacePairs;
    searchReplacePairs.insert(oldQtPath, newQtPathWithNormalSlashes);
    searchReplacePairs.insert(QByteArray(oldQtPath).replace("/", "\\"), newQtPathWithNormalSlashes);
    searchReplacePairs.insert(QByteArray(oldQtPath).replace("\\", "/"), newQtPathWithNormalSlashes);

#ifdef Q_OS_WIN
    QByteArray newQtPathWithDoubleBackSlashes = QByteArray(newQtPathWithNormalSlashes).replace("/", "\\\\");
    searchReplacePairs.insert(QByteArray(oldQtPath).replace("/", "\\\\"), newQtPathWithDoubleBackSlashes);
    searchReplacePairs.insert(QByteArray(oldQtPath).replace("\\", "\\\\"), newQtPathWithDoubleBackSlashes);

    //this is checking for a possible drive letter, which could be upper or lower
    if (oldQtPath.mid(1,1) == ":") {
        QHash<QByteArray, QByteArray> tempSearchReplacePairs;
        QHashIterator<QByteArray, QByteArray> it(searchReplacePairs);
        QByteArray driveLetter = oldQtPath.left(1);
        while (it.hasNext()) {
            it.next();
            QByteArray currentPossibleSearchByteArrayWithoutDriveLetter = QByteArray(it.key()).remove(0, 1);
            tempSearchReplacePairs.insert(driveLetter.toLower()
                + currentPossibleSearchByteArrayWithoutDriveLetter, it.value());
            tempSearchReplacePairs.insert(driveLetter.toUpper()
                + currentPossibleSearchByteArrayWithoutDriveLetter, it.value());
        }
        searchReplacePairs = tempSearchReplacePairs;
    }
#endif

    // get file list defined by filters and patch them
    if (filters.count() > 0) {
        const QStringList filteredContent = getDirContent(prefix, filters);
        foreach (const QString &fileName, filteredContent) {
            if (QFile::exists(fileName)) {
                QtPatch::patchTextFile(fileName, searchReplacePairs);
            }
        }
    }

    // patch single items
    foreach (QString fileName, textFilesToPatch) {
        fileName.prepend(prefix);

        if (QFile::exists(fileName)) {
            //TODO: use the return value for an error message at the end of the operation
            QtPatch::patchTextFile(fileName, searchReplacePairs);
        }
    }
//END - patch text files

    if (oldQtPathFromQMakeIsEmpty) {
        setError(UserDefinedError);
        setErrorString(tr("The installer was not able to get the unpatched path from \n%1.(maybe it is "
            "broken or removed)\nIt tried to patch the Qt binaries, but all other files in Qt are unpatched."
            "\nThis could result in a broken Qt version.\nSometimes it helps to restart the installer with a "
            "switched off antivirus software.").arg(QDir::toNativeSeparators(qmakePath)));
        return false;
    }

    return true;
}

QStringList QtPatchOperation::getDirContent(const QString& aPath, QStringList aFilters)
{
    QStringList list;
    QDirIterator dirIterator(aPath, aFilters, QDir::AllDirs|QDir::Files|QDir::NoSymLinks,
                             QDirIterator::Subdirectories);
    while (dirIterator.hasNext()) {
        dirIterator.next();
        if (!dirIterator.fileInfo().isDir()) {
            list.append(dirIterator.fileInfo().absoluteFilePath());
            qDebug() << QString::fromLatin1("QtPatchOperation::getDirContent match: '%1'").arg(dirIterator.fileInfo().absoluteFilePath());
        }
    }

    return list;
}

bool QtPatchOperation::undoOperation()
{
    return true;
}

bool QtPatchOperation::testOperation()
{
    return true;
}

Operation *QtPatchOperation::clone() const
{
    return new QtPatchOperation();
}

