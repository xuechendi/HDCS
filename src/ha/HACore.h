#ifndef HDCS_HA_CORE_H
#define HDCS_HA_CORE_H

#include "common/AioCompletionImp.h"
#include "Network/hdcs_networking.h"
#include "common/HeartBeat/HeartBeatService.h"

#include "ha/HDCSCoreStatController.h"
#include "ha/HAConfig.h"

#include <iostream>

namespace hdcs {
typedef std::map<std::string, networking::Connection*> name_to_conn_map_t;
typedef uint8_t HDCS_HA_MSG_TYPE;
#define HDCS_MSG_HEARTBEAT  0X31
#define HDCS_MSG_CORE_STAT  0X32
#define HDCS_MSG_DOMAIN_MAP 0X33
#define HDCS_MSG_CMD        0X34

namespace ha {

class HACore {
public:
  HACore (std::string config_path, std::string port) :
    ha_config(config_path),
    listener("0.0.0.0", port, 1, 1) {
    listener.start([&](void* p, std::string s){request_handler(p, s);});
    listener.async_run();
  }

  ~HACore () {
    printf("do destruction\n");
    listener.stop();
    for (auto &it : conn_map) {
      it.second->close();
    }
  }

  //virtual void apply_domain_map_item (HDCS_DOMAIN_MAP_ITEM domain_map_item) = 0;
  virtual void handle_heartbeat_request (void* session_arg, std::string msg_content) = 0;
  virtual void handle_core_stat_request (void* session_arg, std::string msg_content) = 0;

  void request_handler (void* session_arg, std::string msg_content) {
    // use different msg handler to handle
    HDCS_HA_MSG_TYPE type = *((HDCS_HA_MSG_TYPE*)msg_content.c_str());
    switch (type) {
      // 1. HeartBeat
      case HDCS_MSG_HEARTBEAT:
        handle_heartbeat_request(session_arg, msg_content);
        break;    
      // 2. domain map
      case HDCS_MSG_DOMAIN_MAP:
        //apply_domain_map_item (HDCS_DOMAIN_MAP_ITEM domain_map_item)
        break;    
      // 3. cmdline
      case HDCS_MSG_CMD:

        break;    
      // 4. core_stat
      case HDCS_MSG_CORE_STAT:
        handle_core_stat_request(session_arg, msg_content);
        //update_global_stat_map(core_stat_controller.get_stat_map());
        break;    
      default:
        break;
    }
  }

  void update_global_stat_map (hdcs_core_stat_map_t stat_map) {
    // update node status
    // generate new domain_map

    // distribute domain map
    /*for (auto &it : global_domain_map) {
      conn->aio_communicate(domain_item);
    }*/
  }

  void generate_domain_map () {
    // generate domain map by current stat map
  }

  networking::server listener;
  name_to_conn_map_t conn_map;
  HAConfig ha_config;
  //HACmdHandler cmd_handler;

  //HDCS_GLOBAL_STAT_MAP global_stat_map;
  //HDCS_DOMAIN_MAP global_domain_map;
};
}// ha
}// hdcs
#endif
