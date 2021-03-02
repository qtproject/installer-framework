/**************************************************************************
**
** Copyright (C) 2021 The Qt Company Ltd.
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

#include "lib7zarchive.h"

#include "errors.h"
#include "lib7z_facade.h"
#include "lib7z_create.h"
#include "lib7z_list.h"

#include <QCoreApplication>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Lib7zArchive
    \brief The Lib7zArchive class represents an archive file
           handled with the LZMA software development kit.
*/

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::Lib7zArchive::ExtractCallbackWrapper
    \internal
*/

/*!
    Constructs an archive object representing an archive file
    specified by \a filename with \a parent as parent object.
*/
Lib7zArchive::Lib7zArchive(const QString &filename, QObject *parent)
    : AbstractArchive(parent)
    , m_extractCallback(new ExtractCallbackWrapper())
{
    Lib7zArchive::setFilename(filename);
    listenExtractCallback();
}

/*!
    Constructs an archive object with the given \a parent.
*/
Lib7zArchive::Lib7zArchive(QObject *parent)
    : AbstractArchive(parent)
    , m_extractCallback(new ExtractCallbackWrapper())
{
    listenExtractCallback();
}

/*!
    Destroys the instance and releases resources.
*/
Lib7zArchive::~Lib7zArchive()
{
    delete m_extractCallback;
}

/*!
    \reimp

    Opens the underlying file device using \a mode. Returns \c true if
    succesfull; otherwise \c false.
*/
bool Lib7zArchive::open(QIODevice::OpenMode mode)
{
    if (!m_file.open(mode)) {
        setErrorString(m_file.errorString());
        return false;
    }
    return true;
}

/*!
    \reimp

    Closes the underlying file device.
*/
void Lib7zArchive::close()
{
    m_file.close();
}

/*!
    \reimp

    Sets the \a filename of the underlying file device.
*/
void Lib7zArchive::setFilename(const QString &filename)
{
    m_file.setFileName(filename);
}

/*!
    \reimp

    Extracts the contents of this archive to \a dirPath.
    Returns \c true on success; \c false otherwise.
*/
bool Lib7zArchive::extract(const QString &dirPath)
{
    m_extractCallback->setState(S_OK);
    try {
        Lib7z::extractArchive(&m_file, dirPath, m_extractCallback);
    } catch (const Lib7z::SevenZipException &e) {
        setErrorString(e.message());
        return false;
    }
    return true;
}

/*!
    \reimp

    Extracts the contents of this archive to \a dirPath. The \a totalFiles
    parameter is unused. Returns \c true on success; \c false otherwise.
*/
bool Lib7zArchive::extract(const QString &dirPath, const quint64 totalFiles)
{
    Q_UNUSED(totalFiles)
    return extract(dirPath);
}

/*!
    \reimp

    Packages the given \a data into the archive and creates the file on disk.
*/
bool Lib7zArchive::create(const QStringList &data)
{
    try {
        // No support for callback yet.
        Lib7z::createArchive(&m_file, data, compressionLevel());
    } catch (const Lib7z::SevenZipException &e) {
        setErrorString(e.message());
        return false;
    }
    return true;
}

/*!
    \reimp

    Returns the contents of this archive as an array of \c ArchiveEntry objects.
    On failure, returns an empty array.
*/
QVector<ArchiveEntry> Lib7zArchive::list()
{
    try {
        return Lib7z::listArchive(&m_file);
    } catch (const Lib7z::SevenZipException &e) {
        setErrorString(e.message());
        return QVector<ArchiveEntry>();
    }
}

/*!
    \reimp

    Returns \c true if the current archive is of supported format;
    \c false otherwise.
*/
bool Lib7zArchive::isSupported()
{
    try {
        return Lib7z::isSupportedArchive(&m_file);
    } catch (const Lib7z::SevenZipException &e) {
        setErrorString(e.message());
        return false;
    }
}

/*!
    \reimp

    Cancels the extract operation in progress.
*/
void Lib7zArchive::cancel()
{
    m_extractCallback->setState(E_ABORT);
}

/*!
    \internal
*/
void Lib7zArchive::listenExtractCallback()
{
    connect(m_extractCallback, &ExtractCallbackWrapper::currentEntryChanged,
            this, &Lib7zArchive::currentEntryChanged);
    connect(m_extractCallback, &ExtractCallbackWrapper::completedChanged,
            this, &Lib7zArchive::completedChanged);
}


Lib7zArchive::ExtractCallbackWrapper::ExtractCallbackWrapper()
    : m_state(S_OK)
{
}

void Lib7zArchive::ExtractCallbackWrapper::setState(HRESULT state)
{
    m_state = state;
}

void Lib7zArchive::ExtractCallbackWrapper::setCurrentFile(const QString &filename)
{
    emit currentEntryChanged(filename);
}

HRESULT Lib7zArchive::ExtractCallbackWrapper::setCompleted(quint64 completed, quint64 total)
{
    qApp->processEvents();

    emit completedChanged(completed, total);
    return m_state;
}

} // namespace QInstaller
