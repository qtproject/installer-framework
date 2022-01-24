/**************************************************************************
**
** Copyright (C) 2022 The Qt Company Ltd.
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
#ifndef LIB7Z_GUID_H
#define LIB7Z_GUID_H

#include <Common/MyInitGuid.h>

#ifdef __cplusplus
#define DEFINE_7Z_GUID(name, b4, b5, b6) extern "C" const GUID IID_ ## name = { 0x23170F69, \
    0x40C1, 0x278A, { 0x00, 0x00, 0x00,  b4,  b5,  b6, 0x00, 0x00 } }
#else
#define DEFINE_7Z_GUID(name, b4, b5, b6) extern const GUID IID_ ## name = { 0x23170F69, \
    0x40C1, 0x278A, { 0x00, 0x00, 0x00,  b4,  b5,  b6, 0x00, 0x00 } }
#endif

DEFINE_7Z_GUID(IInArchiveGetStream,             0x06, 0x00, 0x40);
DEFINE_7Z_GUID(IInArchive,                      0x06, 0x00, 0x60);

DEFINE_7Z_GUID(IOutStream,                      0x03, 0x00, 0x04);
DEFINE_7Z_GUID(IOutStreamFlush,                 0x03, 0x00, 0x07);
DEFINE_7Z_GUID(IOutArchive,                     0x06, 0x00, 0xA0);

DEFINE_7Z_GUID(IInStream,                       0x03, 0x00, 0x03);

DEFINE_7Z_GUID(ISetProperties,                  0x06, 0x00, 0x03);

DEFINE_7Z_GUID(ISequentialInStream,             0x03, 0x00, 0x01);
DEFINE_7Z_GUID(ISequentialOutStream,            0x03, 0x00, 0x02);

DEFINE_7Z_GUID(IStreamGetSize,                  0x03, 0x00, 0x06);
DEFINE_7Z_GUID(IStreamGetProps,                 0x03, 0x00, 0x08);
DEFINE_7Z_GUID(IStreamGetProps2,                0x03, 0x00, 0x09);

DEFINE_7Z_GUID(IArchiveKeepModeForNextOpen,     0x06, 0x00, 0x04);
DEFINE_7Z_GUID(IArchiveAllowTail,               0x06, 0x00, 0x05);
DEFINE_7Z_GUID(IArchiveOpenCallback,            0x06, 0x00, 0x10);
DEFINE_7Z_GUID(IArchiveExtractCallback,         0x06, 0x00, 0x20);
DEFINE_7Z_GUID(IArchiveOpenVolumeCallback,      0x06, 0x00, 0x30);
DEFINE_7Z_GUID(IArchiveOpenSetSubArchiveName,   0x06, 0x00, 0x50);
DEFINE_7Z_GUID(IArchiveOpenSeq,                 0x06, 0x00, 0x61);
DEFINE_7Z_GUID(IArchiveGetRawProps,             0x06, 0x00, 0x70);
DEFINE_7Z_GUID(IArchiveGetRootProps,            0x06, 0x00, 0x71);
DEFINE_7Z_GUID(IArchiveUpdateCallback,          0x06, 0x00, 0x80);
DEFINE_7Z_GUID(IArchiveUpdateCallback2,         0x06, 0x00, 0x82);

DEFINE_7Z_GUID(ICompressProgressInfo,             0x04, 0x00, 0x04);
DEFINE_7Z_GUID(ICompressCoder,                    0x04, 0x00, 0x05);
DEFINE_7Z_GUID(ICompressSetCoderProperties,       0x04, 0x00, 0x20);
DEFINE_7Z_GUID(ICompressSetDecoderProperties2,    0x04, 0x00, 0x22);
DEFINE_7Z_GUID(ICompressWriteCoderProperties,     0x04, 0x00, 0x23);
DEFINE_7Z_GUID(ICompressGetInStreamProcessedSize, 0x04, 0x00, 0x24);
DEFINE_7Z_GUID(ICompressSetCoderMt,               0x04, 0x00, 0x25);
DEFINE_7Z_GUID(ICompressSetOutStream,             0x04, 0x00, 0x32);
DEFINE_7Z_GUID(ICompressSetInStream,              0x04, 0x00, 0x31);
DEFINE_7Z_GUID(ICompressSetOutStreamSize,         0x04, 0x00, 0x34);
DEFINE_7Z_GUID(ICompressSetBufSize,               0x04, 0x00, 0x35);
DEFINE_7Z_GUID(ICompressGetSubStreamSize,         0x04, 0x00, 0x30);
DEFINE_7Z_GUID(ICryptoResetInitVector,            0x04, 0x00, 0x8C);
DEFINE_7Z_GUID(ICryptoSetPassword,                0x04, 0x00, 0x90);

DEFINE_7Z_GUID(ICryptoGetTextPassword,            0x05, 0x00, 0x10);
DEFINE_7Z_GUID(ICryptoGetTextPassword2,           0x05, 0x00, 0x11);

#undef DEFINE_7Z_GUID

#endif // LIB7Z_GUID_H
