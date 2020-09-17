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

#include "unziptask.h"
#include "lib7z_facade.h"

#ifdef Q_OS_UNIX
# include "StdAfx.h"
#endif
#include "Common/ComTry.h"
// TODO: include once we switch from lib7z_fascade.h
//#include "Common/MyInitGuid.h"

#include "7zip/Archive/IArchive.h"
#include "7zip/Common/FileStreams.h"
#include "7zip/UI/Common/OpenArchive.h"

#include "Windows/FileDir.h"
#include "Windows/PropVariant.h"

#include <QDir>

namespace QInstaller {

/*!
    \inmodule QtInstallerFramework
    \class QInstaller::UnzipTask
    \internal
*/

class ArchiveExtractCallback : public IArchiveExtractCallback, public CMyUnknownImp
{
public:
    MY_UNKNOWN_IMP
    ArchiveExtractCallback(const QString &target, const CArc &arc, QFutureInterface<QString> &fi)
        : m_target(target)
        , m_arc(arc)
    {
        m_futureInterface = &fi;
    }


    // -- IProgress

    STDMETHOD(SetTotal)(UInt64 total)
    {
        COM_TRY_BEGIN
        m_totalUnpacked = 0;
        m_totalUnpackedExpected = total;
        return S_OK;
        COM_TRY_END
    }

    STDMETHOD(SetCompleted)(const UInt64 *completeValue)
    {
        COM_TRY_BEGIN
        Q_UNUSED(completeValue)
        return S_OK;    // return S_OK here as we do not support sub archive extracting
        COM_TRY_END
    }


    // -- IArchiveExtractCallback

    STDMETHOD(GetStream)(UInt32 index, ISequentialOutStream **outStream,  Int32 askExtractMode)
    {
        if (m_futureInterface->isCanceled())
            return E_FAIL;
        if (m_futureInterface->isPaused())
            m_futureInterface->waitForResume();

        COM_TRY_BEGIN
        *outStream = nullptr;
        m_currentIndex = index;
        if (askExtractMode != NArchive::NExtract::NAskMode::kExtract)
            return E_FAIL;

        bool isDir = false;
        if (Archive_IsItem_Folder(m_arc.Archive, m_currentIndex, isDir) != S_OK)
            return E_FAIL;

        bool isEncrypted = false;
        if (Archive_GetItemBoolProp(m_arc.Archive, m_currentIndex, kpidEncrypted, isEncrypted) != S_OK)
            return E_FAIL;

        if (isDir || isEncrypted)
            return E_FAIL;

        UString itemPath;
        if (m_arc.GetItemPath(m_currentIndex, itemPath) != S_OK)
            return E_FAIL;

        QDir().mkpath(m_target);
        m_currentTarget = m_target + QLatin1Char('/') + QString::fromStdWString((const wchar_t*)(itemPath))
            .replace(QLatin1Char('\\'), QLatin1Char('/'));

        m_outFileStream = new COutFileStream;
        CMyComPtr<ISequentialOutStream> scopedPointer(m_outFileStream);
        if (!m_outFileStream->Open((wchar_t*)(m_currentTarget.utf16()), CREATE_ALWAYS))
            return E_FAIL;

        m_outFileStreamComPtr = scopedPointer;
        *outStream = scopedPointer.Detach();

        return S_OK;
        COM_TRY_END
    }

    STDMETHOD(PrepareOperation)(Int32 askExtractMode)
    {
        COM_TRY_BEGIN
        Q_UNUSED(askExtractMode)
        m_futureInterface->setProgressValueAndText(0, QLatin1String("Started to extract archive."));
        return S_OK;    // return S_OK here as we do not need to prepare anything
        COM_TRY_END
    }

    STDMETHOD(SetOperationResult)(Int32 resultEOperationResult)
    {
        COM_TRY_BEGIN
        switch (resultEOperationResult)
        {
            case NArchive::NExtract::NOperationResult::kOK:
                break;

            default:    // fall through and bail
            case NArchive::NExtract::NOperationResult::kCRCError:
            case NArchive::NExtract::NOperationResult::kDataError:
            case NArchive::NExtract::NOperationResult::kUnsupportedMethod:
                m_outFileStream->Close();
                m_outFileStreamComPtr.Release();
                return E_FAIL;
        }

        UInt32 attributes;
        if (GetAttributes(&attributes))
            NWindows::NFile::NDir::SetFileAttrib((wchar_t*)(m_currentTarget.utf16()), attributes);

        FILETIME accessTime, creationTime, modificationTime;
        const bool writeAccessTime = GetTime(kpidATime, &accessTime);
        const bool writeCreationTime = GetTime(kpidCTime, &creationTime);
        const bool writeModificationTime = GetTime(kpidMTime, &modificationTime);

        m_outFileStream->SetTime((writeCreationTime ? &creationTime : nullptr),
            (writeAccessTime ? &accessTime : nullptr), (writeModificationTime ? &modificationTime : nullptr));

        m_totalUnpacked += m_outFileStream->ProcessedSize;
        m_outFileStream->Close();
        m_outFileStreamComPtr.Release();

        m_futureInterface->reportResult(m_currentTarget);
        m_futureInterface->setProgressValueAndText(100 * m_totalUnpacked / m_totalUnpackedExpected,
            m_currentTarget);
        return S_OK;
        COM_TRY_END
    }

private:
    bool GetAttributes(UInt32 *attributes) const
    {
        *attributes = 0;
        NWindows::NCOM::CPropVariant property;
        if (m_arc.Archive->GetProperty(m_currentIndex, kpidAttrib, &property) != S_OK)
            return false;

        if (property.vt != VT_UI4)
            return false;

        *attributes = property.ulVal;
        return (property.vt == VT_UI4);
    }

    bool GetTime(PROPID propertyId, FILETIME *filetime) const
    {
        if (!filetime)
            return false;

        filetime->dwLowDateTime = 0;
        filetime->dwHighDateTime = 0;

        NWindows::NCOM::CPropVariant property;
        if (m_arc.Archive->GetProperty(m_currentIndex, propertyId, &property) != S_OK)
            return false;

        if (property.vt != VT_FILETIME)
            return false;

        *filetime = property.filetime;
        return (filetime->dwHighDateTime != 0 || filetime->dwLowDateTime != 0);
    }

private:
    QString m_target;
    const CArc &m_arc;
    QFutureInterface<QString> *m_futureInterface;

    UInt32 m_currentIndex;
    QString m_currentTarget;

    UInt64 m_totalUnpacked;
    UInt64 m_totalUnpackedExpected;

    COutFileStream *m_outFileStream;
    CMyComPtr<ISequentialOutStream> m_outFileStreamComPtr;
};


// -- UnzipTask

UnzipTask::UnzipTask(const QString &source, const QString &target)
    : m_source(source)
    , m_target(target)
{
    Lib7z::initSevenZ();
}

void UnzipTask::doTask(QFutureInterface<QString> &fi)
{
    fi.reportStarted();

    CCodecs codecs;
    if (codecs.Load() != S_OK)
        return;


    COpenOptions op;
    op.codecs = &codecs;

    CObjectVector<COpenType> types;
    op.types = &types;  // Empty, because we use a stream.

    CIntVector excluded;
    op.excludedFormats = &excluded;

    const CMyComPtr<CInFileStream> fileStream = new CInFileStream;
    fileStream->Open((wchar_t*) (m_source.utf16()));
    op.stream = fileStream; // CMyComPtr is needed, otherwise it crashes in OpenStream().

    CObjectVector<CProperty> properties;
    op.props = &properties;

    CArchiveLink archiveLink;
    if (archiveLink.Open2(op, nullptr) != S_OK)
        return;

    UINT32 count = 0;
    for (unsigned i = 0; i < archiveLink.Arcs.Size(); ++i) {
        const CArc& arc = archiveLink.Arcs[i];
        UInt32 numItems = 0;
        if (arc.Archive->GetNumberOfItems(&numItems) != S_OK)
            break;
        count += numItems;
    }
    fi.setExpectedResultCount(count);

    for (unsigned i = 0; i < archiveLink.Arcs.Size(); ++i) {
        if (fi.isCanceled())
            break;
        if (fi.isPaused())
            fi.waitForResume();

        const UInt32 extractAll = UInt32(-1);
        const CArc &arc = archiveLink.Arcs[i];
        arc.Archive->Extract(0, extractAll, NArchive::NExtract::NAskMode::kExtract,
            new ArchiveExtractCallback(m_target, arc, fi));
    }

    fi.reportFinished();
}

}   // namespace QInstaller
