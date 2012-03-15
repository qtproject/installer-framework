isEmpty(7ZIP_BASE): 7ZIP_BASE = $$(7ZIP_BASE)
isEmpty(7ZIP_BASE): error(Please call qmake with 7ZIP_BASE=<path to nokia-sdk source directory> or add this line before you include that file in your pro file)

7ZIP_BASE=$$7ZIP_BASE/unix

DEFINES += _FILE_OFFSET_BITS=64 _LARGEFILE_SOURCE NDEBUG _REENTRANT ENV_UNIX BREAK_HANDLER UNICODE _UNICODE

macx:DEFINES += ENV_MACOSX

CXXFLAGS += -fvisibility

INCLUDEPATH += $$7ZIP_BASE/CPP \
    $$7ZIP_BASE/CPP/myWindows \
    $$7ZIP_BASE/CPP/include_windows

SOURCES += $$7ZIP_BASE/CPP/myWindows/myGetTickCount.cpp \
                $$7ZIP_BASE/CPP/myWindows/wine_date_and_time.cpp \
                $$7ZIP_BASE/CPP/myWindows/myAddExeFlag.cpp \
                $$7ZIP_BASE/CPP/myWindows/mySplitCommandLine.cpp \

SOURCES += \
$$7ZIP_BASE/CPP/7zip/UI/Console/ConsoleClose.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/ExtractCallbackConsole.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/List.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/OpenCallbackConsole.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/PercentPrinter.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/UpdateCallbackConsole.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Console/UserInputUtils.cpp \
$$7ZIP_BASE/CPP/Common/CommandLineParser.cpp \
$$7ZIP_BASE/CPP/Common/CRC.cpp \
$$7ZIP_BASE/CPP/Common/IntToString.cpp \
$$7ZIP_BASE/CPP/Common/ListFileUtils.cpp \
$$7ZIP_BASE/CPP/Common/StdInStream.cpp \
$$7ZIP_BASE/CPP/Common/StdOutStream.cpp \
$$7ZIP_BASE/CPP/Common/MyString.cpp \
$$7ZIP_BASE/CPP/Common/StringToInt.cpp \
$$7ZIP_BASE/CPP/Common/UTFConvert.cpp \
$$7ZIP_BASE/CPP/Common/StringConvert.cpp \
$$7ZIP_BASE/CPP/Common/MyWindows.cpp \
$$7ZIP_BASE/CPP/Common/MyVector.cpp \
$$7ZIP_BASE/CPP/Common/Wildcard.cpp \
$$7ZIP_BASE/CPP/Windows/Error.cpp \
$$7ZIP_BASE/CPP/Windows/FileDir.cpp \
$$7ZIP_BASE/CPP/Windows/FileFind.cpp \
$$7ZIP_BASE/CPP/Windows/FileIO.cpp \
$$7ZIP_BASE/CPP/Windows/FileName.cpp \
$$7ZIP_BASE/CPP/Windows/PropVariant.cpp \
$$7ZIP_BASE/CPP/Windows/PropVariantConversions.cpp \
$$7ZIP_BASE/CPP/Windows/Synchronization.cpp \
$$7ZIP_BASE/CPP/Windows/System.cpp \
$$7ZIP_BASE/CPP/Windows/Time.cpp \
$$7ZIP_BASE/CPP/7zip/Common/CreateCoder.cpp \
$$7ZIP_BASE/CPP/7zip/Common/CWrappers.cpp \
$$7ZIP_BASE/CPP/7zip/Common/FilePathAutoRename.cpp \
$$7ZIP_BASE/CPP/7zip/Common/FileStreams.cpp \
$$7ZIP_BASE/CPP/7zip/Common/FilterCoder.cpp \
$$7ZIP_BASE/CPP/7zip/Common/InBuffer.cpp \
$$7ZIP_BASE/CPP/7zip/Common/InOutTempBuffer.cpp \
$$7ZIP_BASE/CPP/7zip/Common/LimitedStreams.cpp \
$$7ZIP_BASE/CPP/7zip/Common/LockedStream.cpp \
$$7ZIP_BASE/CPP/7zip/Common/MemBlocks.cpp \
$$7ZIP_BASE/CPP/7zip/Common/MethodId.cpp \
$$7ZIP_BASE/CPP/7zip/Common/MethodProps.cpp \
$$7ZIP_BASE/CPP/7zip/Common/OffsetStream.cpp \
$$7ZIP_BASE/CPP/7zip/Common/OutBuffer.cpp \
$$7ZIP_BASE/CPP/7zip/Common/OutMemStream.cpp \
$$7ZIP_BASE/CPP/7zip/Common/ProgressMt.cpp \
$$7ZIP_BASE/CPP/7zip/Common/ProgressUtils.cpp \
$$7ZIP_BASE/CPP/7zip/Common/StreamBinder.cpp \
$$7ZIP_BASE/CPP/7zip/Common/StreamObjects.cpp \
$$7ZIP_BASE/CPP/7zip/Common/StreamUtils.cpp \
$$7ZIP_BASE/CPP/7zip/Common/VirtThread.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveCommandLine.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveExtractCallback.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveOpenCallback.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/DefaultName.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/EnumDirItems.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/Extract.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/ExtractingFilePath.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/LoadCodecs.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/OpenArchive.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/PropIDUtils.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/SetProperties.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/SortUtils.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/TempFiles.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/Update.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/UpdateAction.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/UpdateCallback.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/UpdatePair.cpp \
$$7ZIP_BASE/CPP/7zip/UI/Common/UpdateProduce.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/DeflateProps.cpp \ #new
$$7ZIP_BASE/CPP/7zip/Archive/Bz2Handler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/GzHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/LzmaHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/SplitHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/XzHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/ZHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2MT.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/CrossThreadProgress.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/DummyOutStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/FindSignature.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/HandlerOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/InStreamWithCRC.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/ItemNameUtils.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/MultiStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/OutStreamWithCRC.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Common/ParseProperties.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zCompressionMode.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zDecode.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zEncode.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zExtract.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zFolderInStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zFolderOutStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zHandlerOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zHeader.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zIn.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zProperties.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zSpecStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zUpdate.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Cab/CabBlockInStream.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Cab/CabHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Cab/CabHeader.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Cab/CabIn.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHandlerOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarHeader.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarIn.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarUpdate.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipAddCommon.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHandler.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHandlerOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipHeader.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipIn.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipItem.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipOut.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipUpdate.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BcjCoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Bcj2Coder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BitlDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BranchCoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BranchMisc.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/ByteSwap.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BZip2Crc.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BZip2Decoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BZip2Encoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/CopyCoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/DeflateDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/DeflateEncoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/DeltaFilter.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/ImplodeDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/ImplodeHuffmanDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Lzma2Decoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Lzma2Encoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LzmaDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LzmaEncoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LzOutWindow.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Lzx86Converter.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LzxDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/PpmdDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/PpmdEncoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/PpmdZip.cpp \ #new
$$7ZIP_BASE/CPP/7zip/Compress/QuantumDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/ShrinkDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/ZDecoder.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LZMA_Alone/LzmaBench.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LZMA_Alone/LzmaBenchCon.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/7zAes.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/HmacSha1.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/MyAes.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/Pbkdf2HmacSha1.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/RandGen.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/Sha1.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/WzAes.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/ZipCrypto.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/ZipStrong.cpp \
$$7ZIP_BASE/C/7zStream.c \
$$7ZIP_BASE/C/Aes.c \
$$7ZIP_BASE/C/Bra.c \
$$7ZIP_BASE/C/Bra86.c \
$$7ZIP_BASE/C/BraIA64.c \
$$7ZIP_BASE/C/BwtSort.c \
$$7ZIP_BASE/C/Delta.c \
$$7ZIP_BASE/C/HuffEnc.c \
$$7ZIP_BASE/C/LzFind.c \
$$7ZIP_BASE/C/LzFindMt.c \
$$7ZIP_BASE/C/Lzma2Dec.c \
$$7ZIP_BASE/C/Lzma2Enc.c \
$$7ZIP_BASE/C/LzmaDec.c \
$$7ZIP_BASE/C/LzmaEnc.c \
$$7ZIP_BASE/C/MtCoder.c \
$$7ZIP_BASE/C/Sha256.c \
$$7ZIP_BASE/C/Sort.c \
$$7ZIP_BASE/C/Threads.c \
$$7ZIP_BASE/C/Xz.c \
$$7ZIP_BASE/C/XzCrc64.c \
$$7ZIP_BASE/C/XzDec.c \
$$7ZIP_BASE/C/XzEnc.c \
$$7ZIP_BASE/C/XzIn.c \
$$7ZIP_BASE/C/7zCrc.c \
$$7ZIP_BASE/C/Ppmd7Enc.c \ #new
$$7ZIP_BASE/C/Ppmd7Dec.c \ #new
$$7ZIP_BASE/C/Ppmd7.c \ #new
$$7ZIP_BASE/C/Ppmd8.c \ #new
$$7ZIP_BASE/C/Ppmd8Enc.c \ #new
$$7ZIP_BASE/C/Ppmd8Dec.c \ #new
$$7ZIP_BASE/C/Alloc.c \ #new
$$7ZIP_BASE/C/7zCrcOpt.c \ #new
$$7ZIP_BASE/CPP/7zip/Archive/7z/7zRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Cab/CabRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Tar/TarRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Archive/Zip/ZipRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Bcj2Register.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BcjRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BranchRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/BZip2Register.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/CopyRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Deflate64Register.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/DeflateRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/Lzma2Register.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/LzmaRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Compress/PpmdRegister.cpp \
$$7ZIP_BASE/CPP/7zip/Crypto/7zAesRegister.cpp


