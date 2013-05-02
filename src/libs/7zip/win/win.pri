#$(COMMON_OBJS): ../../../Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/Common/IntToString.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/MyString.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StringConvert.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/StringToInt.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/MyVector.cpp
SOURCES += $$7ZIP_BASE/CPP/Common/Wildcard.cpp

#$(WIN_OBJS): ../../../Windows/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/DLL.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileDir.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileFind.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileName.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/FileIO.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/PropVariant.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/PropVariantConversions.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/System.cpp
SOURCES += $$7ZIP_BASE/CPP/Windows/Time.cpp

#$(7ZIP_COMMON_OBJS): ../../Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/CreateCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/CWrappers.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/InBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/InOutTempBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/FileStreams.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/FilterCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/LimitedStreams.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/LockedStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/MethodId.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/MethodProps.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/OutBuffer.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/ProgressUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamBinder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamObjects.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/StreamUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Common/VirtThread.cpp

#$(UI_COMMON_OBJS): ../../UI/Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/ArchiveOpenCallback.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/DefaultName.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/EnumDirItems.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/LoadCodecs.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/OpenArchive.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/SetProperties.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/SortUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/TempFiles.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/Update.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateAction.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateCallback.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdatePair.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/UI/Common/UpdateProduce.cpp

#$(AR_OBJS): ../../Archive/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/LzmaHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/SplitHandler.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/XzHandler.cpp

!static:DEF_FILE += $$7ZIP_BASE/CPP/7zip/Archive/Archive.def
!static:DEF_FILE += $$7ZIP_BASE/CPP/7zip/Archive/Archive2.def

#$(AR_COMMON_OBJS): ../../Archive/Common/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/CoderMixer2MT.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/HandlerOut.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/ItemNameUtils.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/ParseProperties.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/InStreamWithCRC.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/OutStreamWithCRC.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/MultiStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/Common/DummyOutStream.cpp

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
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zSpecStream.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zUpdate.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Archive/7z/7zRegister.cpp

#$(COMPRESS_OBJS): ../../Compress/$(*B).cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Bcj2Coder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Bcj2Register.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BcjCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BcjRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchMisc.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/BranchRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/ByteSwap.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/CopyCoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/CopyRegister.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/DeltaFilter.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Decoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Encoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/Lzma2Register.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaDecoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaEncoder.cpp
SOURCES += $$7ZIP_BASE/CPP/7zip/Compress/LzmaRegister.cpp

#$(C_OBJS): ../../../../C/$(*B).c
SOURCES += $$7ZIP_BASE/C/Alloc.c
SOURCES += $$7ZIP_BASE/C/Bra.c
SOURCES += $$7ZIP_BASE/C/Bra86.c
SOURCES += $$7ZIP_BASE/C/BraIA64.c
SOURCES += $$7ZIP_BASE/C/CpuArch.c
SOURCES += $$7ZIP_BASE/C/Delta.c
SOURCES += $$7ZIP_BASE/C/LzFind.c
SOURCES += $$7ZIP_BASE/C/LzFindMt.c
SOURCES += $$7ZIP_BASE/C/Lzma2Dec.c
SOURCES += $$7ZIP_BASE/C/Lzma2Enc.c
SOURCES += $$7ZIP_BASE/C/LzmaDec.c
SOURCES += $$7ZIP_BASE/C/LzmaEnc.c
SOURCES += $$7ZIP_BASE/C/MtCoder.c
SOURCES += $$7ZIP_BASE/C/Threads.c
SOURCES += $$7ZIP_BASE/C/7zCrc.c
SOURCES += $$7ZIP_BASE/C/7zCrcOpt.c
SOURCES += $$7ZIP_BASE/C/7zStream.c
SOURCES += $$7ZIP_BASE/C/Xz.c
SOURCES += $$7ZIP_BASE/C/XzIn.c
SOURCES += $$7ZIP_BASE/C/XzCrc64.c
SOURCES += $$7ZIP_BASE/C/XzDec.c
SOURCES += $$7ZIP_BASE/C/XzEnc.c
SOURCES += $$7ZIP_BASE/C/Sha256.c
