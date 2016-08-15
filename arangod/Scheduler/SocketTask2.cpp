////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#include "SocketTask2.h"

#include "Basics/StringBuffer.h"
#include "Basics/socket-utils.h"
#include "Logger/Logger.h"
#include "Scheduler/Scheduler.h"

#include <errno.h>

using namespace arangodb::basics;
using namespace arangodb::rest;

SocketTask2::SocketTask2(EventLoop2 loop, TRI_socket_t socket,
                         double keepAliveTimeout)
    : Task2(loop, "SocketTask2"),
#if 0
      _keepAliveTimeout(keepAliveTimeout),
      _clientClosed(false),
      _tid(0),
#endif
      _stream(loop._ioService) {
  _readBuffer.reset(new StringBuffer(TRI_UNKNOWN_MEM_ZONE, false));

  ConnectionStatisticsAgent::acquire();
  connectionStatisticsAgentSetStart();

  boost::system::error_code ec;
  _stream.assign(boost::asio::ip::tcp::v4(), socket.fileDescriptor, ec);

  if (ec) {
    LOG(ERR) << "cannot create stream from socket" << ec;
    _closedSend = true;
    _closedReceive = true;
  } else {
    LOG(ERR) << "ASSIGNED " << socket.fileDescriptor;
  }
}

void SocketTask2::start() {
  if (_closedSend) {
    LOG(DEBUG) << "cannot start, channel closed for send";
    return;
  }

  if (_closeRequested) {
    LOG(DEBUG) << "cannot start, close alread in progress";
    return;
  }

  LOG(ERR) << "### startRead";
  _loop._ioService.post([this]() { syncReadSome(); });
}

void SocketTask2::syncReadSome() {
  while (true) {
    // reserve some memory for reading
    if (_readBuffer->reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
      LOG(WARN) << "out of memory while reading from client";
      closeStream();
      return;
    }

    boost::system::error_code ec;
    size_t n = boost::asio::read(
        _stream, boost::asio::buffer(_readBuffer->end(), READ_BLOCK_SIZE),
        [](const boost::system::error_code const& ec, std::size_t transferred) {
          return 0 < transferred ? 0 : READ_BLOCK_SIZE;
        },
        ec);

    if (ec) {
      LOG(DEBUG) << "read on stream " << _stream.native_handle()
                 << " failed with " << ec;
      closeStream();
      return;
    }

    LOG(ERR) << "read # " << n << "bytes";

    _readBuffer->increaseLength(n);

    while (processRead()) {
      LOG(ERR) << "STILL PROCESSING";

      if (_closeRequested) {
        break;
      }
    }

    LOG(ERR) << "PROCESSING DONE";

    if (_closeRequested) {
      closeSendStream();
      return;
    }
  }
}

void SocketTask2::asyncReadSome() {
  // reserve some memory for reading
  if (_readBuffer->reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    LOG(WARN) << "out of memory while reading from client";
    closeStream();
    return;
  }

  // try to read more bytes
  _stream.async_read_some(
      boost::asio::buffer(_readBuffer->end(), READ_BLOCK_SIZE),
      [this](const boost::system::error_code& ec, std::size_t transferred) {
        if (ec) {
          LOG(DEBUG) << "read on stream " << _stream.native_handle()
                     << " failed with " << ec;
          closeStream();
        } else {
          _readBuffer->increaseLength(transferred);

          while (processRead()) {
            if (_closeRequested) {
              break;
            }
          }

          if (_closeRequested) {
            closeSendStream();
          } else {
            asyncReadSome();
          }
        }
      });
}

void SocketTask2::closeStream() {
  boost::system::error_code ec;
  _stream.shutdown(boost::asio::ip::tcp::socket::shutdown_both);

  if (ec) {
    LOG(WARN) << "shutdown stream " << _stream.native_handle()
              << " failed with " << ec;
  }

  _closedSend = true;
  _closedReceive = true;
  _closeRequested = false;
}

void SocketTask2::closeSendStream() {
  boost::system::error_code ec;
  _stream.shutdown(boost::asio::ip::tcp::socket::shutdown_send);

  if (ec) {
    LOG(WARN) << "shutdown send stream " << _stream.native_handle()
              << " failed with " << ec;
  }

  _closedSend = true;
  _closeRequested = false;
}

#if 0

////////////////////////////////////////////////////////////////////////////////
/// @brief deletes a socket task
///
/// This method will close the underlying socket.
////////////////////////////////////////////////////////////////////////////////

SocketTask2::~SocketTask2() {
  if (TRI_isvalidsocket(_commSocket)) {
    TRI_CLOSE_SOCKET(_commSocket);
    TRI_invalidatesocket(&_commSocket);
  }

  delete _writeBuffer;

  if (_writeBufferStatistics != nullptr) {
    TRI_ReleaseRequestStatistics(_writeBufferStatistics);
  }

  connectionStatisticsAgentSetEnd();
  ConnectionStatisticsAgent::release();
}

void SocketTask2::setKeepAliveTimeout(double timeout) {
  if (_keepAliveWatcher != nullptr && timeout > 0.0) {
    _scheduler->rearmTimer(_keepAliveWatcher, timeout);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief fills the read buffer
////////////////////////////////////////////////////////////////////////////////

bool SocketTask2::fillReadBuffer() {
  // reserve some memory for reading
  if (_readBuffer->reserve(READ_BLOCK_SIZE + 1) == TRI_ERROR_OUT_OF_MEMORY) {
    // out of memory
    LOG(TRACE) << "out of memory";

    return false;
  }

  int nr = TRI_READ_SOCKET(_commSocket, _readBuffer->end(), READ_BLOCK_SIZE, 0);

  if (nr > 0) {
    _readBuffer->increaseLength(nr);
    _readBuffer->ensureNullTerminated();
    return true;
  }

  if (nr == 0) {
    LOG(TRACE) << "read returned 0";
    _clientClosed = true;

    return false;
  }

  int myerrno = errno;

  if (myerrno == EINTR) {
    // read interrupted by signal
    return fillReadBuffer();
  }

  // condition is required like this because g++ 6 will complain about
  //   if (myerrno != EWOULDBLOCK && myerrno != EAGAIN)
  // having two identical branches (because EWOULDBLOCK == EAGAIN on Linux).
  // however, posix states that there may be systems where EWOULDBLOCK !=
  // EAGAIN...
  if (myerrno != EWOULDBLOCK && (EWOULDBLOCK == EAGAIN || myerrno != EAGAIN)) {
    LOG(DEBUG) << "read from socket failed with " << myerrno << ": "
               << strerror(myerrno);

    return false;
  }

  TRI_ASSERT(myerrno == EWOULDBLOCK ||
             (EWOULDBLOCK != EAGAIN && myerrno == EAGAIN));

  // from man(2) read:
  // The  file  descriptor  fd  refers  to  a socket and has been marked
  // nonblocking (O_NONBLOCK),
  // and the read would block.  POSIX.1-2001 allows
  // either error to be returned for this case, and does not require these
  // constants to have the same value,
  // so a  portable  application  should check for both possibilities.
  LOG(TRACE) << "read would block with " << myerrno << ": "
             << strerror(myerrno);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief handles a write
////////////////////////////////////////////////////////////////////////////////

bool SocketTask2::handleWrite() {
  size_t len = 0;

  if (nullptr != _writeBuffer) {
    TRI_ASSERT(_writeBuffer->length() >= _writeLength);
    len = _writeBuffer->length() - _writeLength;
  }

  int nr = 0;

  if (0 < len) {
    nr = TRI_WRITE_SOCKET(_commSocket, _writeBuffer->begin() + _writeLength,
                          (int)len, 0);

    if (nr < 0) {
      int myerrno = errno;

      if (myerrno == EINTR) {
        // write interrupted by signal
        return handleWrite();
      }

      if (myerrno != EWOULDBLOCK &&
          (EAGAIN == EWOULDBLOCK || myerrno != EAGAIN)) {
        LOG(DEBUG) << "writing to socket failed with " << myerrno << ": "
                   << strerror(myerrno);

        return false;
      }

      TRI_ASSERT(myerrno == EWOULDBLOCK ||
                 (EWOULDBLOCK != EAGAIN && myerrno == EAGAIN));
      nr = 0;
    }

    TRI_ASSERT(nr >= 0);

    len -= nr;
  }

  if (len == 0) {
    delete _writeBuffer;
    _writeBuffer = nullptr;

    completedWriteBuffer();

    // rearm timer for keep-alive timeout
    setKeepAliveTimeout(_keepAliveTimeout);
  } else {
    _writeLength += nr;
  }

  if (_clientClosed) {
    return false;
  }

  // we might have a new write buffer or none at all
  if (_writeBuffer == nullptr) {
    _scheduler->stopSocketEvents(_writeWatcher);
  } else {
    _scheduler->startSocketEvents(_writeWatcher);
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an active write buffer
////////////////////////////////////////////////////////////////////////////////
#endif

void SocketTask2::setWriteBuffer(StringBuffer* buffer,
                                 TRI_request_statistics_t* statistics) {
  LOG(ERR) << "setWriteBuffer";

  TRI_ASSERT(buffer != nullptr);

  _writeBufferStatistics = statistics;

  if (_writeBufferStatistics != nullptr) {
    _writeBufferStatistics->_writeStart = TRI_StatisticsTime();
    _writeBufferStatistics->_sentBytes += buffer->length();
  }

  if (buffer->empty()) {
    delete buffer;

    completedWriteBuffer();
  } else {
    delete _writeBuffer;

    _writeBuffer = buffer;
  }

  if (_closedSend) {
    return;
  }

  if (_writeBuffer != nullptr) {
#if 0
    boost::asio::async_write(
        _stream,
        boost::asio::buffer(_writeBuffer->begin(), _writeBuffer->length()),
        [this](const boost::system::error_code& ec, std::size_t transferred) {
          if (ec) {
            LOG(DEBUG) << "read on stream " << _stream.native_handle()
                       << " failed with " << ec;
            closeStream();
          } else {
            delete _writeBuffer;
            _writeBuffer = nullptr;

            completedWriteBuffer();
          }
        });
#else
    LOG(ERR) << "completed write buffer";
    
    boost::system::error_code ec;

    size_t n = boost::asio::write(
        _stream,
        boost::asio::buffer(_writeBuffer->begin(), _writeBuffer->length()), ec);

    if (ec) {
      LOG(DEBUG) << "read on stream " << _stream.native_handle()
                 << " failed with " << ec;
      closeStream();
    } else {
      delete _writeBuffer;
      _writeBuffer = nullptr;

      completedWriteBuffer();
    }
#endif
  }
}

#if 0
////////////////////////////////////////////////////////////////////////////////
/// @brief checks for presence of an active write buffer
////////////////////////////////////////////////////////////////////////////////


bool SocketTask2::setup(Scheduler* scheduler, EventLoop loop) {
#ifdef _WIN32
  // ..........................................................................
  // The problem we have here is that this opening of the fs handle may fail.
  // There is no mechanism to the calling function to report failure.
  // ..........................................................................
  LOG(TRACE) << "attempting to convert socket handle to socket descriptor";

  if (!TRI_isvalidsocket(_commSocket)) {
    LOG(ERR) << "In SocketTask2::setup could not convert socket handle to "
                "socket descriptor -- invalid socket handle";
    return false;
  }

  // For the official version of libev we would do this:
  // int res = _open_osfhandle(_commSocket.fileHandle, 0);
  // However, this opens a whole lot of problems and in general one should
  // never use _open_osfhandle for sockets.
  // Therefore, we do the following, although it has the potential to
  // lose the higher bits of the socket handle:
  int res = (int)_commSocket.fileHandle;

  if (res == -1) {
    LOG(ERR) << "In SocketTask2::setup could not convert socket handle to "
                "socket descriptor -- _open_osfhandle(...) failed";
    res = TRI_CLOSE_SOCKET(_commSocket);

    if (res != 0) {
      res = WSAGetLastError();
      LOG(ERR)
          << "In SocketTask2::setup closesocket(...) failed with error code: "
          << res;
    }

    TRI_invalidatesocket(&_commSocket);
    return false;
  }

  _commSocket.fileDescriptor = res;

#endif

  _scheduler = scheduler;
  _loop = loop;

  // install timer for keep-alive timeout with some high default value
  _keepAliveWatcher = _scheduler->installTimerEvent(loop, this, 60.0);

  // and stop it immediately so it's not active at the start
  _scheduler->clearTimer(_keepAliveWatcher);

  _tid = Thread::currentThreadId();

  _writeWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_WRITE, this,
                                                 _commSocket);

  _readWatcher = _scheduler->installSocketEvent(loop, EVENT_SOCKET_READ, this,
                                                _commSocket);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief cleans up the task by unregistering all watchers
////////////////////////////////////////////////////////////////////////////////

void SocketTask2::cleanup() {
  if (_scheduler != nullptr) {
    if (_keepAliveWatcher != nullptr) {
      _scheduler->uninstallEvent(_keepAliveWatcher);
    }

    if (_readWatcher != nullptr) {
      _scheduler->uninstallEvent(_readWatcher);
    }

    if (_writeWatcher != nullptr) {
      _scheduler->uninstallEvent(_writeWatcher);
    }
  }

  _keepAliveWatcher = nullptr;
  _readWatcher = nullptr;
  _writeWatcher = nullptr;
}

bool SocketTask2::handleEvent(EventToken token, EventType revents) {
  bool result = true;

  if (token == _keepAliveWatcher && (revents & EVENT_TIMER)) {
    // got a keep-alive timeout
    LOG(TRACE) << "got keep-alive timeout signal, closing connection";

    _scheduler->clearTimer(token);

    // this will close the connection and destroy the task
    handleTimeout();
    return false;
  }

  if (token == _readWatcher && (revents & EVENT_SOCKET_READ)) {
    if (_keepAliveWatcher != nullptr) {
      // disable timer for keep-alive timeout
      _scheduler->clearTimer(_keepAliveWatcher);
    }

    result = handleRead();
  }

  if (result && !_clientClosed && token == _writeWatcher) {
    if (revents & EVENT_SOCKET_WRITE) {
      result = handleWrite();
    }
  }

  if (result) {
    if (_writeBuffer == nullptr) {
      _scheduler->stopSocketEvents(_writeWatcher);
    } else {
      _scheduler->startSocketEvents(_writeWatcher);
    }
  }

  return result;
}

#endif
