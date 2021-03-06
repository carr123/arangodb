////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_ARANGO_GLOBAL_CONTEXT_H
#define ARANGODB_BASICS_ARANGO_GLOBAL_CONTEXT_H 1

#include "Basics/Common.h"

namespace arangodb {
class ArangoGlobalContext {
 public:
  static ArangoGlobalContext* CONTEXT;

 public:
  ArangoGlobalContext(int argc, char* argv[]);
  ~ArangoGlobalContext();

 public:
  std::string binaryName() { return _binaryName; }
  int exit(int ret);
  void installHup();
  void installSegv();
  void maskAllSignals();
  void unmaskStandardSignals();
  void runStartupChecks();
  void tempPathAvailable();
  bool useEventLog() { return _useEventLog; } 

 private:
  std::string _binaryName;
  int _ret;
  bool _useEventLog;
};
}

#endif
