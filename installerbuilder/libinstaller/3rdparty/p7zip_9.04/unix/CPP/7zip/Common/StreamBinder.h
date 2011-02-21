// StreamBinder.h

#ifndef __STREAMBINDER_H
#define __STREAMBINDER_H

#include "../IStream.h"
#include "../../Windows/Synchronization.h"

class CStreamBinder
{
#ifdef _WIN32
  NWindows::NSynchronization::CManualResetEvent _allBytesAreWritenEvent;
  NWindows::NSynchronization::CManualResetEvent _thereAreBytesToReadEvent;
  NWindows::NSynchronization::CManualResetEvent _readStreamIsClosedEvent;
#else
  NWindows::NSynchronization::CManualResetEventWFMO _allBytesAreWritenEvent;
  NWindows::NSynchronization::CManualResetEvent _thereAreBytesToReadEvent;
  NWindows::NSynchronization::CManualResetEventWFMO _readStreamIsClosedEvent;
  NWindows::NSynchronization::CSynchro * _synchroFor_allBytesAreWritenEvent_and_readStreamIsClosedEvent;
#endif
  UInt32 _bufferSize;
  const void *_buffer;
public:
  // bool ReadingWasClosed;
  UInt64 ProcessedSize;

#ifndef _WIN32
  CStreamBinder() { _synchroFor_allBytesAreWritenEvent_and_readStreamIsClosedEvent = 0; }
  ~CStreamBinder() {
	if (_synchroFor_allBytesAreWritenEvent_and_readStreamIsClosedEvent)
		delete _synchroFor_allBytesAreWritenEvent_and_readStreamIsClosedEvent;
	_synchroFor_allBytesAreWritenEvent_and_readStreamIsClosedEvent = 0;
  }
#endif

  HRes CreateEvents();

  void CreateStreams(ISequentialInStream **inStream,
      ISequentialOutStream **outStream);
  HRESULT Read(void *data, UInt32 size, UInt32 *processedSize);
  void CloseRead();

  HRESULT Write(const void *data, UInt32 size, UInt32 *processedSize);
  void CloseWrite();
  void ReInit();
};

#endif
