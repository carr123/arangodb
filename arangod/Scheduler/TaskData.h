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

#ifndef ARANGOD_SCHEDULER_TASK_DATA_H
#define ARANGOD_SCHEDULER_TASK_DATA_H 1

#include "Basics/Common.h"

#include "Rest/GeneralResponse.h"
#include "Scheduler/events.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rest {
class TaskData : public RequestStatisticsAgent {
 public:
  static uint64_t const TASK_DATA_RESPONSE = 1000;
  static uint64_t const TASK_DATA_CHUNK = 1001;
  static uint64_t const TASK_DATA_BUFFER = 1002;

 public:
  uint64_t _taskId;
  EventLoop _loop;
  uint64_t _type;
  std::string _data;
  std::unique_ptr<GeneralResponse> _response;
  std::shared_ptr<velocypack::Buffer<uint8_t>> _buffer;
};
}
}

#endif
