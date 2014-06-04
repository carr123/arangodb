////////////////////////////////////////////////////////////////////////////////
/// @brief V8 engine configuration
///
/// @file
///
/// DISCLAIMER
///
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Copyright 2011-2014, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef TRIAGENS_V8SERVER_APPLICATION_V8_H
#define TRIAGENS_V8SERVER_APPLICATION_V8_H 1

#include "Basics/Common.h"

#include "ApplicationServer/ApplicationFeature.h"

#include <v8.h>

#include "Basics/ConditionVariable.h"
#include "V8/JSLoader.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_server_s;
struct TRI_vocbase_s;

namespace triagens {
  namespace basics {
    class Thread;
  }

  namespace rest {
    class HttpRequest;
    class ApplicationDispatcher;
    class ApplicationScheduler;
  }

  namespace arango {

// -----------------------------------------------------------------------------
// --SECTION--                                        class GlobalContextMethods
// -----------------------------------------------------------------------------

    class GlobalContextMethods {
  
      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief method type
////////////////////////////////////////////////////////////////////////////////

        enum MethodType {
          TYPE_UNKNOWN = 0,
          TYPE_RELOAD_ROUTING,
          TYPE_FLUSH_MODULE_CACHE,
          TYPE_RELOAD_AQL
        };

////////////////////////////////////////////////////////////////////////////////
/// @brief get a method type number from a type string
////////////////////////////////////////////////////////////////////////////////

        static MethodType getType (std::string const& type) {
          if (type == "reloadRouting") {
            return TYPE_RELOAD_ROUTING;
          }
          if (type == "flushModuleCache") {
            return TYPE_FLUSH_MODULE_CACHE;
          }
          if (type == "reloadAql") {
            return TYPE_RELOAD_AQL;
          }

          return TYPE_UNKNOWN;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief get code for a method
////////////////////////////////////////////////////////////////////////////////

        static std::string const getCode (MethodType type) {
          switch (type) {
            case TYPE_RELOAD_ROUTING:
              return CodeReloadRouting;
            case TYPE_FLUSH_MODULE_CACHE:
              return CodeFlushModuleCache;
            case TYPE_RELOAD_AQL:
              return CodeReloadAql;
            case TYPE_UNKNOWN:
            default:
              return "";
          }
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief static strings with the code for each method
////////////////////////////////////////////////////////////////////////////////

        static std::string const CodeReloadRouting;
        static std::string const CodeFlushModuleCache;
        static std::string const CodeReloadAql;
    };

// -----------------------------------------------------------------------------
// --SECTION--                                               class ApplicationV8
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief application simple user and session management feature
////////////////////////////////////////////////////////////////////////////////

    class ApplicationV8 : public rest::ApplicationFeature {
      private:
        ApplicationV8 (ApplicationV8 const&);
        ApplicationV8& operator= (ApplicationV8 const&);

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 isolate and context
////////////////////////////////////////////////////////////////////////////////

        struct V8Context {

////////////////////////////////////////////////////////////////////////////////
/// @brief identifier
////////////////////////////////////////////////////////////////////////////////

          size_t _id;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 context
////////////////////////////////////////////////////////////////////////////////

          v8::Persistent<v8::Context> _context;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 isolate
////////////////////////////////////////////////////////////////////////////////

          v8::Isolate* _isolate;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 locker
////////////////////////////////////////////////////////////////////////////////

          v8::Locker* _locker;

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global method
///
/// Caller must hold the _contextCondition.
////////////////////////////////////////////////////////////////////////////////

          bool addGlobalContextMethod (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief executes all global methods
///
/// Caller must hold the _contextCondition.
////////////////////////////////////////////////////////////////////////////////

          void handleGlobalContextMethods ();

////////////////////////////////////////////////////////////////////////////////
/// @brief mutex to protect _globalMethods
////////////////////////////////////////////////////////////////////////////////

          basics::Mutex _globalMethodsLock;

////////////////////////////////////////////////////////////////////////////////
/// @brief open global methods
////////////////////////////////////////////////////////////////////////////////

          std::vector<GlobalContextMethods::MethodType> _globalMethods;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of executions since last GC of the context
////////////////////////////////////////////////////////////////////////////////

          size_t _numExecutions;

////////////////////////////////////////////////////////////////////////////////
/// @brief timestamp of last GC for the context
////////////////////////////////////////////////////////////////////////////////

          double _lastGcStamp;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the context has dead (ex-v8 wrapped) objects
////////////////////////////////////////////////////////////////////////////////

          bool _hasDeadObjects;
        };

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// @brief constructor
////////////////////////////////////////////////////////////////////////////////

        ApplicationV8 (struct TRI_server_s*,
                       rest::ApplicationScheduler*,
                       rest::ApplicationDispatcher*);

////////////////////////////////////////////////////////////////////////////////
/// @brief destructor
////////////////////////////////////////////////////////////////////////////////

        ~ApplicationV8 ();

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return the app-path
////////////////////////////////////////////////////////////////////////////////

        const std::string& appPath () const {
          return _appPath;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief return the dev-app-path
////////////////////////////////////////////////////////////////////////////////

        const std::string& devAppPath () const {
          return _devAppPath;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the concurrency
////////////////////////////////////////////////////////////////////////////////

        void setConcurrency (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief sets the database
////////////////////////////////////////////////////////////////////////////////

        void setVocbase (struct TRI_vocbase_s*);

////////////////////////////////////////////////////////////////////////////////
/// @brief enters an context
////////////////////////////////////////////////////////////////////////////////

        V8Context* enterContext (TRI_vocbase_s*,
                                 triagens::rest::HttpRequest*, 
                                 bool,
                                 bool);

////////////////////////////////////////////////////////////////////////////////
/// @brief exists an context
////////////////////////////////////////////////////////////////////////////////

        void exitContext (V8Context*);

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a global context functions to be executed asap
////////////////////////////////////////////////////////////////////////////////

        bool addGlobalContextMethod (string const&);

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the garbage collection
////////////////////////////////////////////////////////////////////////////////

        void collectGarbage ();

////////////////////////////////////////////////////////////////////////////////
/// @brief disables actions
////////////////////////////////////////////////////////////////////////////////

        void disableActions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief enables development mode
////////////////////////////////////////////////////////////////////////////////

        void enableDevelopmentMode ();

////////////////////////////////////////////////////////////////////////////////
/// @brief defines a boolean variable
////////////////////////////////////////////////////////////////////////////////

        void defineBoolean (const std::string& name, bool value) {
          _definedBooleans[name] = value;
        }

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the version check
////////////////////////////////////////////////////////////////////////////////

        void runVersionCheck (bool skip, bool perform);

////////////////////////////////////////////////////////////////////////////////
/// @brief runs the upgrade check
////////////////////////////////////////////////////////////////////////////////

        void runUpgradeCheck ();

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the actions
////////////////////////////////////////////////////////////////////////////////

        void prepareActions ();

////////////////////////////////////////////////////////////////////////////////
/// @brief sets an alternate init file
///
/// Normally "server.js" will be used. Pass empty string to disable.
////////////////////////////////////////////////////////////////////////////////

        void setStartupFile (const string&);

// -----------------------------------------------------------------------------
// --SECTION--                                        ApplicationFeature methods
// -----------------------------------------------------------------------------

      public:

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void setupOptions (map<string, basics::ProgramOptionsDescription>&);

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool prepare2 ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        bool start ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void close ();

////////////////////////////////////////////////////////////////////////////////
/// {@inheritDoc}
////////////////////////////////////////////////////////////////////////////////

        void stop ();

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

      private:

////////////////////////////////////////////////////////////////////////////////
/// @brief determine which of the free contexts should be picked for the GC
////////////////////////////////////////////////////////////////////////////////

        V8Context* pickFreeContextForGc ();

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares a V8 instance
////////////////////////////////////////////////////////////////////////////////

        bool prepareV8Instance (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief prepares the V8 actions
////////////////////////////////////////////////////////////////////////////////

        void prepareV8Actions (size_t);

////////////////////////////////////////////////////////////////////////////////
/// @brief shut downs a V8 instances
////////////////////////////////////////////////////////////////////////////////

        void shutdownV8Instance (size_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief server object
////////////////////////////////////////////////////////////////////////////////

        struct TRI_server_s* _server;

////////////////////////////////////////////////////////////////////////////////
/// @brief path to the directory containing the startup scripts
///
/// @CMDOPT{\--javascript.startup-directory @CA{directory}}
///
/// Specifies the @CA{directory} path to the JavaScript files used for
/// bootstraping.
////////////////////////////////////////////////////////////////////////////////

        string _startupPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief semicolon separated list of module directories
///
/// This variable is automatically set based on the value of
/// `--javascript.startup-directory`.
////////////////////////////////////////////////////////////////////////////////

        string _modulesPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief path to the system action directory
///
/// This variable is automatically set based on the value of
/// `--javascript.startup-directory`.
////////////////////////////////////////////////////////////////////////////////

        string _actionPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief semicolon separated list of application directories
///
/// @CMDOPT{\--javascript.app-path @CA{directory}}
///
/// Specifies the @CA{directory} path where the applications are located.
/// Multiple paths can be specified separated with commas.
////////////////////////////////////////////////////////////////////////////////

        string _appPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief semicolon separated list of application directories
///
/// @CMDOPT{\--javascript.dev-app-path @CA{directory}}
///
/// Specifies the @CA{directory} path where the development applications are
/// located.  Multiple paths can be specified separated with commas. Never use
/// this option for production.
////////////////////////////////////////////////////////////////////////////////

        string _devAppPath;

////////////////////////////////////////////////////////////////////////////////
/// @brief use actions
////////////////////////////////////////////////////////////////////////////////

        bool _useActions;

////////////////////////////////////////////////////////////////////////////////
/// @brief enables development mode
////////////////////////////////////////////////////////////////////////////////

        bool _developmentMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief enables frontend development mode
////////////////////////////////////////////////////////////////////////////////

        bool _frontendDevelopmentMode;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript garbage collection interval (each x requests)
///
/// @CMDOPT{\--javascript.gc-interval @CA{interval}}
///
/// Specifies the interval (approximately in number of requests) that the
/// garbage collection for JavaScript objects will be run in each thread.
////////////////////////////////////////////////////////////////////////////////

        uint64_t _gcInterval;

////////////////////////////////////////////////////////////////////////////////
/// @brief JavaScript garbage collection frequency (each x seconds)
///
/// @CMDOPT{\--javascript.gc-frequency @CA{frequency}}
///
/// Specifies the frequency (in seconds) for the automatic garbage collection of
/// JavaScript objects. This setting is useful to have the garbage collection
/// still work in periods with no or little numbers of requests.
////////////////////////////////////////////////////////////////////////////////

        double _gcFrequency;

////////////////////////////////////////////////////////////////////////////////
/// @brief optional arguments to pass to v8
///
/// @CMDOPT{\--javascript.v8-options @CA{options}}
///
/// Optional arguments to pass to the V8 Javascript engine. The V8 engine will
/// run with default settings unless explicit options are specified using this
/// option. The options passed will be forwarded to the V8 engine which will
/// parse them on its own. Passing invalid options may result in an error being
/// printed on stderr and the option being ignored.
///
/// Options need to be passed in one string, with V8 option names being prefixed
/// with double dashes. Multiple options need to be separated by whitespace.
/// To get a list of all available V8 options, you can use
/// the value @LIT{"--help"} as follows:
/// @code
/// --javascript.v8-options "--help"
/// @endcode
///
/// Another example of specific V8 options being set at startup:
/// @code
/// --javascript.v8-options "--harmony --log"
/// @endcode
///
/// Names and features or usable options depend on the version of V8 being used,
/// and might change in the future if a different version of V8 is being used
/// in ArangoDB. Not all options offered by V8 might be sensible to use in the
/// context of ArangoDB. Use the specific options only if you are sure that
/// they are not harmful for the regular database operation.
////////////////////////////////////////////////////////////////////////////////

        string _v8Options;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 startup loader
////////////////////////////////////////////////////////////////////////////////

        JSLoader _startupLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 action loader
////////////////////////////////////////////////////////////////////////////////

        JSLoader _actionLoader;

////////////////////////////////////////////////////////////////////////////////
/// @brief system database
////////////////////////////////////////////////////////////////////////////////

        struct TRI_vocbase_s* _vocbase;

////////////////////////////////////////////////////////////////////////////////
/// @brief number of instances to create
////////////////////////////////////////////////////////////////////////////////

        size_t _nrInstances;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 contexts
////////////////////////////////////////////////////////////////////////////////

        V8Context** _contexts;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 contexts queue lock
////////////////////////////////////////////////////////////////////////////////

        basics::ConditionVariable _contextCondition;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 free contexts
////////////////////////////////////////////////////////////////////////////////

        std::vector<V8Context*> _freeContexts;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 free contexts
////////////////////////////////////////////////////////////////////////////////

        std::vector<V8Context*> _dirtyContexts;

////////////////////////////////////////////////////////////////////////////////
/// @brief V8 busy contexts
////////////////////////////////////////////////////////////////////////////////

        std::set<V8Context*> _busyContexts;

////////////////////////////////////////////////////////////////////////////////
/// @brief shutdown in progress
////////////////////////////////////////////////////////////////////////////////

        volatile sig_atomic_t _stopping;

////////////////////////////////////////////////////////////////////////////////
/// @brief garbage collection thread
////////////////////////////////////////////////////////////////////////////////

        basics::Thread* _gcThread;

////////////////////////////////////////////////////////////////////////////////
/// @brief scheduler
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationScheduler* _scheduler;

////////////////////////////////////////////////////////////////////////////////
/// @brief dispatcher
////////////////////////////////////////////////////////////////////////////////

        rest::ApplicationDispatcher* _dispatcher;

////////////////////////////////////////////////////////////////////////////////
/// @brief boolean to be defined
////////////////////////////////////////////////////////////////////////////////

        std::map<std::string, bool> _definedBooleans;

////////////////////////////////////////////////////////////////////////////////
/// @brief startup file
////////////////////////////////////////////////////////////////////////////////

        std::string _startupFile;
    };
  }
}

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
