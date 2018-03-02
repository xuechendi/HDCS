#ifndef HDCS_CORE_STAT_GUARD_H
#define HDCS_CORE_STAT_GUARD_H

#include "HDCSCore.h"
#include "common/Timer.h"
#include <map>

namespace HDCS {
class HDCSCoreStatGuard {
public:
  HDCSCoreStatGuard () {
    std::list<std::string> core_list = data_store_guard.get_unclosed_core_list();
    for (auto core : core_list) {
      // add to HDCSCoreStatMap
      // set opened core state
    }
    
  }

  ~HDCSCoreStatGuard () {

  }

  void register_core (std::string volume_name, HDCSCore* core) {
    // add to HDCSCoreStatMap
    // set core state
    auto it = HDCSCoreStatMap.find( volume_name );
    if (it == HDCSCoreStatMap.end()) {
      HDCSCoreStatMap[volume_name] = HDCSCoreStatMachine(core);
    } else {
      // handle when this volume exists
    }

  }

  void unregister_core (std::string volume_name) {
    // remove from HDCSCoreStatMap
    auto it = HDCSCoreStatMap.find( volume_name );
    if (it == HDCSCoreStatMap.end()) {
      HDCSCoreStatMap.erase(it);
    }
  }

  void refresh_domain () {
    for (auto &core : HDCSCoreStatMap) {
      // set core state
    }

  }

  void handler_core_error (HDCSCore* core, ssize_t errno) {
    // set core state

  }

private:
  std::map<std::string, HDCSCoreStatMachine> HDCSCoreStatMap;
};
}

#endif
