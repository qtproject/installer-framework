isEmpty(7ZIP_BASE): 7ZIP_BASE = $$(7ZIP_BASE)
isEmpty(7ZIP_BASE): error(Please call qmake with 7ZIP_BASE=<path to nokia-sdk source directory> or add this line before you include that file in your pro file)

7ZIP_BASE=$$7ZIP_BASE/win

CONFIG += no_batch # this is needed because we have a same named *.c and *.cpp file -> 7in

DEFINES += WIN_LONG_PATH _UNICODE _CRT_SECURE_NO_WARNINGS

INCLUDEPATH += $$7ZIP_BASE/CPP

#$(CONSOLE_OBJS): ../../UI/Console/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/BenchCon.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/ConsoleClose.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/ExtractCallbackConsole.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/List.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/Main.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/MainAr.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/OpenCallbackConsole.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/PercentPrinter.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/StdAfx.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/UpdateCallbackConsole.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Console/UserInputUtils.cpp

#$(COMMON_OBJS): ../../../Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/Common/CommandLineParser.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/CRC.cpp
#SOURCES += $$7ZIP_BASE/CPP/Common/C_FileIO.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/IntToString.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/ListFileUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/MyString.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/MyVector.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/NewHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StdInStream.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StdOutStream.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StringConvert.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StringToInt.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/UTFConvert.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/Wildcard.cpp

#$(WIN_OBJS): ../../../Windows/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/DLL.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/Error.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileDir.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileFind.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileIO.cpp
#SOURCES += $$7ZIP_BASE/CPP/Windows/FileMapping.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileName.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/MemoryLock.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/PropVariant.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/PropVariantConversions.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/Registry.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/Synchronization.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/System.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/Time.cpp

#$(7ZIP_COMMON_OBJS): ../../Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/CreateCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/CWrappers.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/FilePathAutoRename.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/FileStreams.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/FilterCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/InBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/InOutTempBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/LimitedStreams.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/LockedStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/MemBlocks.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/MethodId.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/MethodProps.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/OffsetStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/OutBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/OutMemStream.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/ProgressUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/ProgressMt.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamBinder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamObjects.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/VirtThread.cpp

#$(UI_COMMON_OBJS): ../../UI/Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveCommandLine.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveExtractCallback.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveName.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveOpenCallback.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/Bench.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/DefaultName.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/EnumDirItems.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/Extract.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ExtractingFilePath.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/LoadCodecs.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/OpenArchive.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/PropIDUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/SetProperties.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/SortUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/TempFiles.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/Update.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateAction.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateCallback.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdatePair.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateProduce.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/WorkDir.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ZipRegistry.cpp

#$(AR_OBJS): ../../Archive/$(*B).cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/ArchiveExports.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/DllExports2.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Bz2Handler.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/DeflateProps.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/GzHandler.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/LzmaHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/SplitHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/XzHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/ZHandler.cpp #added to support more then 7z

#$(AR_COMMON_OBJS): ../../Archive/Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2MT.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/CrossThreadProgress.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/DummyOutStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/FindSignature.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/HandlerOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/InStreamWithCRC.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/ItemNameUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/MultiStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/OutStreamWithCRC.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/ParseProperties.cpp

#$(7Z_OBJS): ../../Archive/7z/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zCompressionMode.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zDecode.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zEncode.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zExtract.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zFolderInStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zFolderOutStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zHandlerOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zHeader.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zIn.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zProperties.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zSpecStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zUpdate.cpp
#SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/StdAfx.cpp

#$(CAB_OBJS): ../../Archive/Cab/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Cab/CabBlockInStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Cab/CabHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Cab/CabHeader.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Cab/CabIn.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Cab/CabRegister.cpp

#$(TAR_OBJS): ../../Archive/Tar/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHandlerOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHeader.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarIn.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Tar/TarUpdate.cpp

#$(ZIP_OBJS): ../../Archive/Zip/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipAddCommon.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHandlerOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHeader.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipIn.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipItem.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipUpdate.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipRegister.cpp

#$(COMPRESS_OBJS): ../../Compress/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Bcj2Coder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Bcj2Register.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BcjCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BcjRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BitlDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchMisc.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ByteSwap.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BZip2Encoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BZip2Decoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BZip2Crc.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BZip2Register.cpp #added to support more then 7z
#SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/CodecExports.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/CopyCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/CopyRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/DeltaFilter.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/DeflateDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/DeflateEncoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Deflate64Register.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/DeflateRegister.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ImplodeHuffmanDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ImplodeDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Decoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Encoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Register.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaDecoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaEncoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzOutWindow.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzx86Converter.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzxDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/PpmdEncoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/PpmdDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/PpmdRegister.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/PpmdZip.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/QuantumDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ShrinkDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ZlibDecoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ZlibEncoder.cpp #added to support more then 7z
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ZDecoder.cpp #added to support more then 7z

#Crypto is not needed for 7z only
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/7zAes.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/7zAesRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/HmacSha1.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/MyAes.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/Pbkdf2HmacSha1.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/RandGen.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/Sha1.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/WzAes.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/ZipCrypto.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Crypto/ZipStrong.cpp

#$(C_OBJS): ../../../../C/$(*B).c
#SOURCES += $$7ZIP_BASE/C/7zAlloc.c
#SOURCES += $$7ZIP_BASE/C/7zBuf.c
#SOURCES += $$7ZIP_BASE/C/7zBuf2.c
SOURCES += $$7ZIP_BASE/C/7zCrc.c
SOURCES += $$7ZIP_BASE/C/7zCrcOpt.c
#SOURCES += $$7ZIP_BASE/C/7zDec.c
#SOURCES += $$7ZIP_BASE/C/7zFile.c
#SOURCES += $$7ZIP_BASE/C/7zIn.c
SOURCES += $$7ZIP_BASE/C/7zStream.c
SOURCES += $$7ZIP_BASE/C/Aes.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/AesOpt.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Alloc.c
#SOURCES += $$7ZIP_BASE/C/Bcj2.c
SOURCES += $$7ZIP_BASE/C/Bra.c
SOURCES += $$7ZIP_BASE/C/Bra86.c
SOURCES += $$7ZIP_BASE/C/BraIA64.c
SOURCES += $$7ZIP_BASE/C/BwtSort.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/CpuArch.c
SOURCES += $$7ZIP_BASE/C/HuffEnc.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Delta.c
SOURCES += $$7ZIP_BASE/C/LzFind.c
SOURCES += $$7ZIP_BASE/C/LzFindMt.c
SOURCES += $$7ZIP_BASE/C/Lzma2Dec.c
SOURCES += $$7ZIP_BASE/C/Lzma2Enc.c
#SOURCES += $$7ZIP_BASE/C/Lzma86Dec.c
#SOURCES += $$7ZIP_BASE/C/Lzma86Enc.c
SOURCES += $$7ZIP_BASE/C/LzmaDec.c
SOURCES += $$7ZIP_BASE/C/LzmaEnc.c
#SOURCES += $$7ZIP_BASE/C/LzmaLib.c
SOURCES += $$7ZIP_BASE/C/MtCoder.c
SOURCES += $$7ZIP_BASE/C/Ppmd7.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Ppmd7Dec.c
SOURCES += $$7ZIP_BASE/C/Ppmd7Enc.c
SOURCES += $$7ZIP_BASE/C/Ppmd8.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Ppmd8Dec.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Ppmd8Enc.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Sha256.c
SOURCES += $$7ZIP_BASE/C/Sort.c #added to support more then 7z
SOURCES += $$7ZIP_BASE/C/Threads.c
SOURCES += $$7ZIP_BASE/C/Xz.c
SOURCES += $$7ZIP_BASE/C/XzCrc64.c
SOURCES += $$7ZIP_BASE/C/XzDec.c
SOURCES += $$7ZIP_BASE/C/XzEnc.c
SOURCES += $$7ZIP_BASE/C/XzIn.c
