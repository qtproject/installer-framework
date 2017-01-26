/****************************************************************************
**
** Copyright (C) 2013 Klaralvdalens Datakonsult AB (KDAB)
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
****************************************************************************/

#include "kdupdaterupdateoperations.h"
#include "errors.h"
#include "fileutils.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QTemporaryFile>
#include <QFileInfo>

#include <cerrno>

using namespace KDUpdater;

static QString errnoToQString(int error)
{
#if defined(Q_OS_WIN) && !defined(Q_CC_MINGW)
    char msg[128];
    if (strerror_s(msg, sizeof msg, error) != 0)
        return QString::fromLocal8Bit(msg);
    return QString();
#else
    return QString::fromLocal8Bit(strerror(error));
#endif
}

static bool removeDirectory(const QString &path, QString *errorString, bool force)
{
    Q_ASSERT(errorString);

    QDir dir = path;
    const QFileInfoList entries = dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden);
    foreach (const QFileInfo &entry, entries) {
        if (entry.isDir() && (!entry.isSymLink()))
            removeDirectory(entry.filePath(), errorString, force);
        else if (force && (!QFile(entry.filePath()).remove()))
            return false;
    }

    // even remove some hidden, OS-created files in there
    QInstaller::removeSystemGeneratedFiles(path);

    errno = 0;
    const bool success = dir.rmdir(path);
    if (errno)
        *errorString = errnoToQString(errno);
    return success;
}

/*
 * \internal
 * Returns a filename for a temporary file based on \a templateName
 */
static QString backupFileName(const QString &templateName)
{
    QTemporaryFile file(templateName);
    file.open();
    const QString name = file.fileName();
    file.close();
    file.remove();
    return name;
}

////////////////////////////////////////////////////////////////////////////
// KDUpdater::CopyOperation
////////////////////////////////////////////////////////////////////////////

CopyOperation::CopyOperation()
{
    setName(QLatin1String("Copy"));
}

CopyOperation::~CopyOperation()
{
    deleteFileNowOrLater(value(QLatin1String("backupOfExistingDestination")).toString());
}

QString CopyOperation::sourcePath()
{
    return arguments().first();
}

QString CopyOperation::destinationPath()
{
    QString destination = arguments().last();

    // if the target is a directory use the source filename to complete the destination path
    if (QFileInfo(destination).isDir())
        destination = QDir(destination).filePath(QFileInfo(sourcePath()).fileName());
    return destination;
}


void CopyOperation::backup()
{
    QString destination = destinationPath();

    if (!QFile::exists(destination)) {
        clearValue(QLatin1String("backupOfExistingDestination"));
        return;
    }

    setValue(QLatin1String("backupOfExistingDestination"), backupFileName(destination));

    // race condition: The backup file could get created by another process right now. But this is the same
    // in QFile::copy...
    if (!QFile::rename(destination, value(QLatin1String("backupOfExistingDestination")).toString()))
        setError(UserDefinedError, tr("Could not backup file %1.").arg(destination));
}

bool CopyOperation::performOperation()
{
    // We need two args to complete the copy operation. First arg provides the complete file name of source
    // Second arg provides the complete file name of dest
    if (arguments().count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 2 expected.").arg(arguments().count()));
        return false;
    }

    QString source = sourcePath();
    QString destination = destinationPath();

    QFile sourceFile(source);
    if (!sourceFile.exists()) {
        setError(UserDefinedError);
        setErrorString(tr("Could not copy a non-existent file: %1").arg(source));
        return false;
    }
    // If destination file exists, we cannot use QFile::copy() because it does not overwrite an existing
    // file. So we remove the destination file.
    QFile destinationFile(destination);
    if (destinationFile.exists()) {
        if (!destinationFile.remove()) {
            setError(UserDefinedError);
            setErrorString(tr("Could not remove destination file %1: %2").arg(destination, destinationFile.errorString()));
            return false;
        }
    }

    const bool copied = sourceFile.copy(destination);
    if (!copied) {
        setError(UserDefinedError);
        setErrorString(tr("Could not copy %1 to %2: %3").arg(source, destination, sourceFile.errorString()));
    }
    return copied;
}

bool CopyOperation::undoOperation()
{
    QString source = sourcePath();
    QString destination = destinationPath();

    // if the target is a directory use the source filename to complete the destination path
    if (QFileInfo(destination).isDir())
        destination = destination + QDir::separator() + QFileInfo(source).fileName();

    QFile destFile(destination);
    // first remove the dest
    if (destFile.exists() && !destFile.remove()) {
        setError(UserDefinedError, tr("Could not delete file %1: %2").arg(destination, destFile.errorString()));
        return false;
    }

    // no backup was done:
    // the copy destination file wasn't existing yet - that's no error
    if (!hasValue(QLatin1String("backupOfExistingDestination")))
        return true;

    QFile backupFile(value(QLatin1String("backupOfExistingDestination")).toString());
    // otherwise we have to copy the backup back:
    const bool success = backupFile.rename(destination);
    if (!success)
        setError(UserDefinedError, tr("Could not restore backup file into %1: %2").arg(destination, backupFile.errorString()));
    return success;
}

/*!
 \reimp
 */
QDomDocument CopyOperation::toXml() const
{
    // we don't want to save the backupOfExistingDestination
    if (!hasValue(QLatin1String("backupOfExistingDestination")))
        return UpdateOperation::toXml();

    CopyOperation *const me = const_cast<CopyOperation *>(this);

    const QVariant v = value(QLatin1String("backupOfExistingDestination"));
    me->clearValue(QLatin1String("backupOfExistingDestination"));
    const QDomDocument xml = UpdateOperation::toXml();
    me->setValue(QLatin1String("backupOfExistingDestination"), v);
    return xml;
}

bool CopyOperation::testOperation()
{
    // TODO
    return true;
}

CopyOperation *CopyOperation::clone() const
{
    return new CopyOperation();
}


////////////////////////////////////////////////////////////////////////////
// KDUpdater::MoveOperation
////////////////////////////////////////////////////////////////////////////

MoveOperation::MoveOperation()
{
    setName(QLatin1String("Move"));
}

MoveOperation::~MoveOperation()
{
    deleteFileNowOrLater(value(QLatin1String("backupOfExistingDestination")).toString());
}

void MoveOperation::backup()
{
    const QString dest = arguments().last();
    if (!QFile::exists(dest)) {
        clearValue(QLatin1String("backupOfExistingDestination"));
        return;
    }

    setValue(QLatin1String("backupOfExistingDestination"), backupFileName(dest));

    // race condition: The backup file could get created by another process right now. But this is the same
    // in QFile::copy...
    if (!QFile::rename(dest, value(QLatin1String("backupOfExistingDestination")).toString()))
        setError(UserDefinedError, tr("Could not backup file %1.").arg(dest));
}

bool MoveOperation::performOperation()
{
    // We need two args to complete the copy operation. // First arg provides the complete file name of
    // source, second arg provides the complete file name of dest
    const QStringList args = this->arguments();
    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 2 expected.").arg(args.count()));
        return false;
    }

    const QString dest = args.last();
    // If destination file exists, then we cannot use QFile::copy() because it does not overwrite an existing
    // file. So we remove the destination file.
    if (QFile::exists(dest)) {
        QFile file(dest);
        if (!file.remove(dest)) {
            setError(UserDefinedError);
            setErrorString(tr("Could not remove destination file %1: %2").arg(dest, file.errorString()));
            return false;
        }
    }

    // Copy source to destination.
    QFile file(args.first());
    if (!file.copy(dest)) {
        setError(UserDefinedError);
        setErrorString(tr("Could not copy %1 to %2: %3").arg(file.fileName(), dest, file.errorString()));
        return false;
    }
    return deleteFileNowOrLater(file.fileName());
}

bool MoveOperation::undoOperation()
{
    const QStringList args = arguments();
    const QString dest = args.last();
    // first: copy back the destination to source
    QFile destF(dest);
    if (!destF.copy(args.first())) {
        setError(UserDefinedError, tr("Cannot copy %1 to %2: %3").arg(dest, args.first(), destF.errorString()));
        return false;
    }

    // second: delete the move destination
    if (!deleteFileNowOrLater(dest)) {
        setError(UserDefinedError, tr("Cannot remove file %1."));
        return false;
    }

    // no backup was done:
    // the move destination file wasn't existing yet - that's no error
    if (!hasValue(QLatin1String("backupOfExistingDestination")))
        return true;

    // otherwise we have to copy the backup back:
    QFile backupF(value(QLatin1String("backupOfExistingDestination")).toString());
    const bool success = backupF.rename(dest);
    if (!success)
        setError(UserDefinedError, tr("Cannot restore backup file for %1: %2").arg(dest, backupF.errorString()));

    return success;
}

bool MoveOperation::testOperation()
{
    // TODO
    return true;
}

MoveOperation *MoveOperation::clone() const
{
    return new MoveOperation;
}


////////////////////////////////////////////////////////////////////////////
// KDUpdater::DeleteOperation
////////////////////////////////////////////////////////////////////////////

DeleteOperation::DeleteOperation()
{
    setName(QLatin1String("Delete"));
}

DeleteOperation::~DeleteOperation()
{
    deleteFileNowOrLater(value(QLatin1String("backupOfExistingFile")).toString());
}

void DeleteOperation::backup()
{
    const QString fileName = arguments().first();
    setValue(QLatin1String("backupOfExistingFile"), backupFileName(fileName));

    QFile file(fileName);
    if (!file.copy(value(QLatin1String("backupOfExistingFile")).toString()))
        setError(UserDefinedError, tr("Cannot create backup of %1: %2").arg(fileName, file.errorString()));
}

bool DeleteOperation::performOperation()
{
    // Requires only one parameter. That is the name of the file to remove.
    const QStringList args = this->arguments();
    if (args.count() != 1) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 1 expected.").arg(args.count()));
        return false;
    }
    return deleteFileNowOrLater(args.first());
}

bool DeleteOperation::undoOperation()
{
    if (!hasValue(QLatin1String("backupOfExistingFile")))
        return true;

    const QString fileName = arguments().first();
    QFile backupF(value(QLatin1String("backupOfExistingFile")).toString());
    const bool success = backupF.copy(fileName) && deleteFileNowOrLater(backupF.fileName());
    if (!success)
        setError(UserDefinedError, tr("Cannot restore backup file for %1: %2").arg(fileName, backupF.errorString()));
    return success;
}

bool DeleteOperation::testOperation()
{
    // TODO
    return true;
}

DeleteOperation *DeleteOperation::clone() const
{
    return new DeleteOperation;
}

/*!
 \reimp
 */
QDomDocument DeleteOperation::toXml() const
{
    // we don't want to save the backupOfExistingFile
    if (!hasValue(QLatin1String("backupOfExistingFile")))
        return UpdateOperation::toXml();

    DeleteOperation *const me = const_cast<DeleteOperation *>(this);

    const QVariant v = value(QLatin1String("backupOfExistingFile"));
    me->clearValue(QLatin1String("backupOfExistingFile"));
    const QDomDocument xml = UpdateOperation::toXml();
    me->setValue(QLatin1String("backupOfExistingFile"), v);
    return xml;
}

////////////////////////////////////////////////////////////////////////////
// KDUpdater::MkdirOperation
////////////////////////////////////////////////////////////////////////////

MkdirOperation::MkdirOperation()
{
    setName(QLatin1String("Mkdir"));
}

void MkdirOperation::backup()
{
    QString path = arguments().first();
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));

    QDir createdDir = QDir::root();

    // find out, which part of the path is the first one we actually need to create
    int end = 0;
    while (true) {
        QString p = path.section(QLatin1Char('/'), 0, ++end);
        createdDir = QDir(p);
        if (!createdDir.exists())
            break;
        if (p == path) {
            // everything did already exist -> nothing to do for us (nothing to revert then, either)
            createdDir = QDir::root();
            break;
        }
    }

    setValue(QLatin1String("createddir"), createdDir.absolutePath());
}

bool MkdirOperation::performOperation()
{
    // Requires only one parameter. That is the path which should be created
    QStringList args = this->arguments();
    if (args.count() != 1) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 1 expected.").arg(args.count()));
        return false;
    }

    const QString dirName = args.first();
    const bool created = QDir::root().mkpath(dirName);
    if (!created) {
        setError(UserDefinedError);
        setErrorString(tr("Could not create folder %1: Unknown error.").arg(dirName));
    }
    return created;
}

bool MkdirOperation::undoOperation()
{
    Q_ASSERT(arguments().count() == 1);

    QString createdDirValue = value(QLatin1String("createddir")).toString();
    if (createdDirValue.isEmpty())
        createdDirValue = arguments().first();
    QDir createdDir = QDir(createdDirValue);
    const bool forceremoval = QVariant(value(QLatin1String("forceremoval"))).toBool();

    // Since refactoring we know the mkdir operation which is creating the target path. If we do a full
    // uninstall prevent removing the full path including target, instead remove the target only. (QTIFW-46)
    if (hasValue(QLatin1String("uninstall-only")) && value(QLatin1String("uninstall-only")).toBool())
        createdDir = QDir(arguments().first());

    if (createdDir == QDir::root())
        return true;

    if (!createdDir.exists())
        return true;

    QString errorString;

    const bool result = removeDirectory(createdDir.path(), &errorString, forceremoval);

    if (!result) {
        if (errorString.isEmpty())
            setError(UserDefinedError, tr("Cannot remove directory %1: %2").arg(createdDir.path(), errorString));
        else
            setError(UserDefinedError, tr("Cannot remove directory %1: %2").arg(createdDir.path(), errnoToQString(errno)));
    }
    return result;
}

bool KDUpdater::MkdirOperation::testOperation()
{
    // TODO
    return true;
}

MkdirOperation *MkdirOperation::clone() const
{
    return new MkdirOperation;
}

////////////////////////////////////////////////////////////////////////////
// KDUpdater::RmdirOperation
////////////////////////////////////////////////////////////////////////////

RmdirOperation::RmdirOperation()
{
    setName(QLatin1String("Rmdir"));
    setValue(QLatin1String("removed"), false);
}

void RmdirOperation::backup()
{
    // nothing to backup - rollback will just create the directory
}

bool RmdirOperation::performOperation()
{
    // Requires only one parameter. That is the name of the file to remove.
    const QStringList args = this->arguments();
    if (args.count() != 1) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 1 expected.").arg(args.count()));
        return false;
    }

    QDir dir(args.first());
    if (!dir.exists()) {
        setError(UserDefinedError);
        setErrorString(tr("Could not remove folder %1: The folder does not exist.").arg(args.first()));
        return false;
    }

    errno = 0;
    const bool removed = dir.rmdir(args.first());
    setValue(QLatin1String("removed"), removed);
    if (!removed) {
        setError(UserDefinedError);
        setErrorString(tr("Could not remove folder %1: %2").arg(args.first(), errnoToQString(errno)));
    }
    return removed;
}

bool RmdirOperation::undoOperation()
{
   if (!value(QLatin1String("removed")).toBool())
        return true;

    errno = 0;
    const QFileInfo fi(arguments().first());
    const bool success = fi.dir().mkdir(fi.fileName());
    if( !success)
        setError(UserDefinedError, tr("Cannot recreate directory %1: %2").arg(fi.fileName(), errnoToQString(errno)));

    return success;
}

bool RmdirOperation::testOperation()
{
    // TODO
    return true;
}

RmdirOperation *RmdirOperation::clone() const
{
    return new RmdirOperation;
}


////////////////////////////////////////////////////////////////////////////
// KDUpdater::AppendFileOperation
////////////////////////////////////////////////////////////////////////////

AppendFileOperation::AppendFileOperation()
{
    setName(QLatin1String("AppendFile"));
}

void AppendFileOperation::backup()
{
    const QString filename = arguments().first();

    QFile file(filename);
    if (!file.exists())
        return; // nothing to backup

    setValue(QLatin1String("backupOfFile"), backupFileName(filename));
    if (!file.copy(value(QLatin1String("backupOfFile")).toString())) {
        setError(UserDefinedError, tr("Cannot backup file %1: %2").arg(filename, file.errorString()));
        clearValue(QLatin1String("backupOfFile"));
    }
}

bool AppendFileOperation::performOperation()
{
    // This operation takes two arguments. First argument is the name of the file into which a text has to be
    // appended. Second argument is the text to append.
    const QStringList args = this->arguments();
    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments in %0: %1 arguments given, %2 expected%3.")
            .arg(name()).arg(arguments().count()).arg(tr("exactly 2"), QLatin1String("")));
        return false;
    }

    const QString fName = args.first();
    QFile file(fName);
    if (!file.open(QFile::Append)) {
        // first we rename the file, then we copy it to the real target and open the copy - the renamed original is then marked for deletion
        bool error = false;
        const QString newName = backupFileName(fName);

        if (!QFile::rename(fName, newName))
            error = true;

        if (!error && !QFile::copy(newName, fName)) {
            error = true;
            QFile::rename(newName, fName);
        }

        if (!error && !file.open(QFile::Append)) {
            error = true;
            deleteFileNowOrLater(newName);
        }

        if (error) {
            setError(UserDefinedError);
            setErrorString(tr("Could not open file '%1' for writing: %2").arg(file.fileName(), file.errorString()));
            return false;
        }
        deleteFileNowOrLater(newName);
    }

    QTextStream ts(&file);
    ts << args.last();
    file.close();

    return true;
}

bool AppendFileOperation::undoOperation()
{
   // backupOfFile being empty -> file didn't exist before -> no error
    const QString filename = arguments().first();
    const QString backupOfFile = value(QLatin1String("backupOfFile")).toString();
    if (!backupOfFile.isEmpty() && !QFile::exists(backupOfFile)) {
        setError(UserDefinedError, tr("Cannot find backup file for %1.").arg(filename));
        return false;
    }

    const bool removed = deleteFileNowOrLater(filename);
    if (!removed) {
        setError(UserDefinedError, tr("Could not restore backup file for %1.").arg(filename));
        return false;
    }

    // got deleted? We might be done, if it didn't exist before
    if (backupOfFile.isEmpty())
        return true;

    QFile backupFile(backupOfFile);
    const bool success = backupFile.rename(filename);
    if (!success)
        setError(UserDefinedError, tr("Could not restore backup file for %1: %2").arg(filename, backupFile.errorString()));
    return success;
}

bool AppendFileOperation::testOperation()
{
    // TODO
    return true;
}

AppendFileOperation *AppendFileOperation::clone() const
{
    return new AppendFileOperation;
}


////////////////////////////////////////////////////////////////////////////
// KDUpdater::PrependFileOperation
////////////////////////////////////////////////////////////////////////////

PrependFileOperation::PrependFileOperation()
{
    setName(QLatin1String("PrependFile"));
}

void PrependFileOperation::backup()
{
    const QString filename = arguments().first();

    QFile file(filename);
    if (!file.exists())
        return; // nothing to backup

    setValue(QLatin1String("backupOfFile"), backupFileName(filename));
    if (!file.copy(value(QLatin1String("backupOfFile")).toString())) {
        setError(UserDefinedError, tr("Cannot backup file %1: %2").arg(filename, file.errorString()));
        clearValue(QLatin1String("backupOfFile"));
    }
}

bool PrependFileOperation::performOperation()
{
    // This operation takes two arguments. First argument is the name
    // of the file into which a text has to be appended. Second argument
    // is the text to append.
    const QStringList args = this->arguments();
    if (args.count() != 2) {
        setError(InvalidArguments);
        setErrorString(tr("Invalid arguments: %1 arguments given, 2 expected.").arg(args.count()));
        return false;
    }

    const QString fName = args.first();
    // Load the file first.
    QFile file(fName);
    if (!file.open(QFile::ReadOnly)) {
        setError(UserDefinedError);
        setErrorString(tr("Could not open file %1 for reading: %2").arg(file.fileName(), file.errorString()));
        return false;
    }

    // TODO: fix this, use a text stream
    QString fContents(QLatin1String(file.readAll()));
    file.close();

    // Prepend text to the file text
    fContents = args.last() + fContents;

    // Now re-open the file in write only mode.
    if (!file.open(QFile::WriteOnly)) {
        // first we rename the file, then we copy it to the real target and open the copy - the renamed original is then marked for deletion
        const QString newName = backupFileName(fName);
        if (!QFile::rename(fName, newName) && QFile::copy(newName, fName) && file.open(QFile::WriteOnly)) {
            QFile::rename(newName, fName);
            setError(UserDefinedError);
            setErrorString(tr("Could not open file %1 for writing: %2").arg(file.fileName(), file.errorString()));
            return false;
        }
        deleteFileNowOrLater(newName);
    }
    QTextStream ts(&file);
    ts << fContents;
    file.close();

    return true;
}

bool PrependFileOperation::undoOperation()
{
    // bockupOfFile being empty -> file didn't exist before -> no error
    const QString filename = arguments().first();
    const QString backupOfFile = value(QLatin1String("backupOfFile")).toString();
    if (!backupOfFile.isEmpty() && !QFile::exists(backupOfFile)) {
        setError(UserDefinedError, tr("Cannot find backup file for %1.").arg(filename));
        return false;
    }

    if (!deleteFileNowOrLater(filename)) {
        setError(UserDefinedError, tr("Cannot restore backup file for %1.").arg(filename));
        return false;
    }

    // got deleted? We might be done, if it didn't exist before
    if (backupOfFile.isEmpty())
        return true;

    QFile backupF(backupOfFile);
    const bool success = backupF.rename(filename);
    if (!success)
        setError(UserDefinedError, tr("Cannot restore backup file for %1: %2").arg(filename, backupF.errorString()));

    return success;
}

bool PrependFileOperation::testOperation()
{
    // TODO
    return true;
}

PrependFileOperation *PrependFileOperation::clone() const
{
    return new PrependFileOperation;
}
