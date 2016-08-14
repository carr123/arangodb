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

#ifndef ARANGOD_SCHEDULER_TASK2_H
#define ARANGOD_SCHEDULER_TASK2_H 1

#include "Basics/Common.h"

#include "Scheduler/TaskData.h"
#include "Scheduler/events.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rest {
class Task2 {
  Task2(Task2 const&) = delete;
  Task2& operator=(Task2 const&) = delete;

 public:
  Task2(EventLoop2, std::string const& name);
  virtual ~Task2() {}

 public:
  // returns the internal task identifier
  uint64_t taskId() const { return _taskId; }

  // returns the task name for debugging
  std::string const& name() const { return _name; }

  // returns the internal event loop
  EventLoop2 eventLoop() const { return _loop; }

  // get a VelocyPack representation of the task for reporting
  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack() const;
  void toVelocyPack(arangodb::velocypack::Builder&) const;

 protected:
  EventLoop2 _loop;
  uint64_t const _taskId;

 private:
  std::string const _name;
};
}
}

#endif
