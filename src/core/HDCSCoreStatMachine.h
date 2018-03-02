#ifndef HDCSCORESTATMACHINE_H
#define HDCSCORESTATMACHINE_H

#include <boost/statechart/event.hpp>
#include <boost/statechart/state_machine.hpp>
#include <boost/statechart/simple_state.hpp>
#include <boost/statechart/transition.hpp>
#include <boost/statechart/custom_reaction.hpp>
#include <ctime>
#include <iostream>
#include "core/HDCSCore.h"

namespace hdcs {
namespace core {

struct EvStatFail : boost::statechart::event< EvStatFail > {};
struct EvStatNext : boost::statechart::event< EvStatNext > {};

struct StatInfo {
  virtual std::string get_stat_name() const=0;
};

struct NotWorking;
struct HDCSCoreStatMachine :
  boost::statechart::state_machine<HDCSCoreStatMachine, NotWorking> {
  public:
    HDCSCoreStatMachine (HDCSCore* core) :
      core(core) {
    }

    std::string get_stat_name () const {
      return state_cast< const StatInfo & >().get_stat_name();
    }

    void set_hdcs_core () {
      
    }

    bool IsHDCSCoreExists () {
      //TODO:  we should check if this core is invalid
      if (core != nullptr) {
        return true;
      } else {
        return false;
      }
    }

    bool IsHDCSCoreReady () {
      if (core->check_data_consistency()) {
        return true;
      } else {
        return false;
      }
    }

    bool IsHDCSCorePeered () {
      if (core->get_peered_core_num() >= core->get_min_replica_size()) {
        return true;
      } else {
        return false;
      }
    }

    bool IsHDCSCoreHealthy () {
      if (core->get_peered_core_num() == core->get_replica_size()) {
        return true;
      } else {
        return false;
      }
    }
  private:
    HDCSCore* core;
};

struct Error;
struct Readonly;

struct NotWorking :
  boost::statechart::simple_state<NotWorking, HDCSCoreStatMachine, Error> {
public:
  typedef boost::statechart::transition< EvStatFail, NotWorking > reactions;

  NotWorking () : statename_("NotWorking"){
  
  }

  virtual std::string get_stat_name() const {
    return statename_;
  }

private:
  //data
  std::string statename_;

};

struct Working :
  boost::statechart::simple_state<Working, HDCSCoreStatMachine, Readonly> {
public:
  typedef boost::statechart::transition< EvStatFail, NotWorking > reactions;

  Working () : statename_("Working") {
  
  }

  virtual std::string get_stat_name() const {
    return statename_;
  }

private:
  //data
  std::string statename_;

};

struct Healthy : StatInfo, boost::statechart::simple_state< Healthy, Working > {
public:
  Healthy() : statename_("Healthy") {}

  ~Healthy() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }
private:
  std::string statename_;
};

struct Degraded : StatInfo, boost::statechart::simple_state< Degraded, Working > {
public:
  //typedef boost::statechart::transition< EvStatNext, Healthy > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Degraded() : statename_("Degraded") {}

  ~Degraded() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  boost::statechart::result react(const EvStatNext &);
private:
  std::string statename_;
};

boost::statechart::result Degraded::react(const EvStatNext &) {
  if (context<HDCSCoreStatMachine>().IsHDCSCoreHealthy()) {
    std::cout << "HDCSCore is Healthy, go next" << std::endl;
    return transit< Healthy >();
  } else {
    std::cout << "HDCSCore is expecting more peers" << std::endl;
    return discard_event();
  }
}

struct Readonly : StatInfo, boost::statechart::simple_state< Readonly, Working > {
public:
  //typedef boost::statechart::transition< EvStatNext, Degraded > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Readonly() : statename_("Readonly") {}

  ~Readonly() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }
  
  boost::statechart::result react(const EvStatNext &);
private:
  std::string statename_;
};

boost::statechart::result Readonly::react(const EvStatNext &) {
  if (context<HDCSCoreStatMachine>().IsHDCSCorePeered()) {
    std::cout << "HDCSCore has enough peers, go next" << std::endl;
    return transit< Degraded >();
  } else {
    std::cout << "HDCSCore is expecting more peers" << std::endl;
    return discard_event();
  }
}

struct Inconsistent : StatInfo, boost::statechart::simple_state< Inconsistent, NotWorking > {
public:
  //typedef boost::statechart::transition< EvStatNext, Working > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Inconsistent() : statename_("Inconsistent") {}

  ~Inconsistent() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  boost::statechart::result react(const EvStatNext &);
private:
  std::string statename_;
};

boost::statechart::result Inconsistent::react(const EvStatNext &) {
  if (context<HDCSCoreStatMachine>().IsHDCSCoreReady()) {
    std::cout << "HDCSCore is Ready, go next" << std::endl;
    return transit< Working >();
  } else {
    std::cout << "HDCSCore is not ready" << std::endl;
    return discard_event();
  }
}

struct Error : StatInfo, boost::statechart::simple_state< Error, NotWorking > {
public:
  //typedef boost::statechart::transition< EvStatNext, Inconsistent > reactions;
  typedef boost::statechart::custom_reaction< EvStatNext > reactions;

  Error() : statename_("Error") {}

  ~Error() {}

  virtual std::string get_stat_name() const {
    return statename_;
  }

  boost::statechart::result react(const EvStatNext &);
private:
  std::string statename_;
};

boost::statechart::result Error::react(const EvStatNext &) {
  if (context<HDCSCoreStatMachine>().IsHDCSCoreExists()) {
    std::cout << "HDCSCore Exists, go next" << std::endl;
    return transit< Inconsistent >();
  } else {
    std::cout << "HDCSCore unable to find" << std::endl;
    return discard_event();
  }
}

}
}
#endif
