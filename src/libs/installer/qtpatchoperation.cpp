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
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
**
** $QT_END_LICENSE$
**
**************************************************************************/

#include "qtpatchoperation.h"
#include "qtpatch.h"
#ifdef Q_OS_MAC
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
        const QHash<QString, QByteArray> &qmakeValueHash)
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
    replaceHash.insert(QByteArray("qt_lbexpath=%1").replace("%1", oldValue),
        QByteArray("qt_lbexpath=%1/libexec").replace("%1/", newQtPath + nativeSeperator));

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
        QByteArray("qt_hlibpath=%1/lib").replace("%1/", newQtPath));

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

    // the possible 2 argument case is here to support old syntax
    if (arguments().count() < 2 || arguments().count() > 3) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 3"), QLatin1String("")));
        return false;
    }

    QString type = arguments().at(0);
    bool isPlatformSupported = type.contains(QLatin1String("linux"), Qt::CaseInsensitive)
        || type.contains(QLatin1String("windows"), Qt::CaseInsensitive)
        || type.contains(QLatin1String("mac"), Qt::CaseInsensitive);
    if (!isPlatformSupported) {
        setError(InvalidArguments);
        setErrorString(tr("First argument should be 'linux', 'mac' or 'windows'. No other type is supported "
            "at this time."));
        return false;
    }

    const QString newQtPathStr = QDir::toNativeSeparators(arguments().at(1));
    const QByteArray newQtPath = newQtPathStr.toUtf8();

    QString qmakePath = QString::fromUtf8(newQtPath) + QLatin1String("/bin/qmake");
#ifdef Q_OS_WIN
    qmakePath = qmakePath + QLatin1String(".exe");
#endif

    if (!QFile::exists(qmakePath)) {
        setError(UserDefinedError);
        setErrorString(tr("QMake from the current Qt version \n(%1)is not existing. Please file a bugreport "
            "with this dialog at https://bugreports.qt-project.org.").arg(QDir::toNativeSeparators(qmakePath)));
        return false;
    }

    QByteArray qmakeOutput;
    QHash<QString, QByteArray> qmakeValueHash = QtPatch::qmakeValues(qmakePath, &qmakeOutput);

    if (qmakeValueHash.isEmpty()) {
        setError(UserDefinedError);
        setErrorString(tr("The output of \n%1 -query\nis not parseable. Please file a bugreport with this "
            "dialog https://bugreports.qt-project.org.\noutput: \"%2\"").arg(QDir::toNativeSeparators(qmakePath),
            QString::fromUtf8(qmakeOutput)));
        return false;
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

#ifdef Q_OS_MAC
    // just try to patch here at the beginning to keep the unpatched qmake if mac install_names_tool fails
    MacReplaceInstallNamesOperation operation;
    operation.setArguments(QStringList()
                           //can not use the old path which is wrong in the webkit case
                           //<< QString::fromUtf8(oldQtPath)
                           << QLatin1String("/lib/Qt") // search string
                           << newQtPathStr + QLatin1String("/lib/Qt") //replace string
                           << newQtPathStr //where
                           );
    if (!operation.performOperation()) {
        setError(operation.error());
        setErrorString(operation.errorString());
        return false;
    }
#endif

    QString fileName;
    if (type == QLatin1String("windows"))
        fileName = QString::fromLatin1(":/files-to-patch-windows");
    else if (type == QLatin1String("linux"))
        fileName = QString::fromLatin1(":/files-to-patch-linux");
    else if (type == QLatin1String("linux-emb-arm"))
        fileName = QString::fromLatin1(":/files-to-patch-linux-emb-arm");
    else if (type == QLatin1String("mac"))
        fileName = QString::fromLatin1(":/files-to-patch-macx");

    QFile patchFileListFile(fileName);
    QString version = arguments().value(2).toLower();
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
    QHash<QByteArray, QByteArray> patchValueHash = generatePatchValueHash(newQtPath, qmakeValueHash);

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

