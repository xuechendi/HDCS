#ifndef HDCS_HA_CLIENT_H
#define HDCS_HA_CLIENT_H

#include "ha/HACore.h"
#include "common/HeartBeat/HeartBeatService.h"
#include "ha/HDCSCoreStatController.h"
#include "common/AioCompletionImp.h"
#include <iostream>

namespace hdcs {
namespace ha {

class HAClient : public HACore {
public:
  HAClient (std::string config_path = ""):
    HACore (config_path, "11001"),
    hb_service(std::move(HeartBeatOpts(1000000000, 1000000000))) {
  }

  ~HAClient () {
  }

  void add_ha_server (std::string node) {
    //create connection to ha_server
    int colon_pos;
    colon_pos = node.find(':');
    std::string addr = node.substr(0, colon_pos);
    std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
    //not same as current messenger, dehao will fix network later.
    auto it = conn_map.insert(std::pair<std::string, networking::Connection*>(node,
      new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1)));
    it.first->second->connect(addr, port);

    // provide this connection to hdcs_core_stat_controller
    core_stat_controller.set_conn(it.first->second);

    // clean old conn
    for (name_to_conn_map_t::iterator it = conn_map.begin();
      it != conn_map.end();) {
      name_to_conn_map_t::iterator tmp = it++;
      if (tmp->first.compare(node) != 0) {
        rm_ha_server(tmp);
      }
    }
  }

  void rm_ha_server (std::string node) {
    // disconnect connection with ha_server
    conn_map.erase(node);  
  }

  void rm_ha_server (name_to_conn_map_t::iterator it) {
    // disconnect connection with ha_server
    conn_map.erase(it);
  }

  std::shared_ptr<HDCSCoreStat> register_core (void* hdcs_core_id) {
    return core_stat_controller.register_core(hdcs_core_id);
  }

  void unregister_core (void* hdcs_core_id) {
    core_stat_controller.unregister_core(hdcs_core_id);
  }

  /*void apply_domain_map_item (HDCS_DOMAIN_MAP_ITEM domain_map_item) {
    // 1. notify HDCS controller 
  }*/

  void handle_heartbeat_request (void* session_arg, std::string msg_content) {
    hb_service.request_handler(session_arg, msg_content, &listener);
  }

  void handle_core_stat_request (void* session_arg, std::string msg_content) {
    core_stat_controller.request_handler(session_arg, msg_content); 
  }
private:
  HeartBeatService hb_service;
  name_to_conn_map_t conn_map;
  HDCSCoreStatController core_stat_controller;

  //HDCS_DOMAIN_MAP_ITEM domain_map_item;
};

}// ha
}// hdcs

#endif
