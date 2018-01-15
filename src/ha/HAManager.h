#ifndef HDCS_HA_MANAGER_H
#define HDCS_HA_MANAGER_H

#include "ha/HACore.h"
#include "common/HeartBeat/HeartBeatService.h"
#include "ha/HDCSCoreStatController.h"
#include "common/AioCompletionImp.h"
#include <iostream>

namespace hdcs {
namespace ha {

class HAManager : public HACore{
public:
  HAManager (std::string config_path = "") :
    HACore(config_path, "10001"),
    hb_worker(std::move(HeartBeatOpts(1000000000, 1000000000))){
  }

  ~HAManager () {
  }

  void register_hdcs_node (std::string node) {
    //create conn
    int colon_pos;
    colon_pos = node.find(':');
    std::string addr = node.substr(0, colon_pos);
    std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
    //not same as current messenger, dehao will fix network later.
    auto it = conn_map.insert(std::pair<std::string, networking::Connection*>(node,
      new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1)));
    it.first->second->connect(addr, port);

    // register to Heartbeat
    std::shared_ptr<hdcs::AioCompletion> err_handler = std::make_shared<hdcs::AioCompletionImp>([node](ssize_t r){
     printf("bad news, heartbeat to %s failed.\n", node.c_str());    
     // update to ha_core
     //update_node_status(node, HDCS_HEARTBEAT_FAIL);
    }, -1, false);
    hb_worker.register_node(it.first->second, node, err_handler);
  }

  void unregister_hdcs_node (std::string node) {
    //unregister from HeartBeat
    hb_worker.unregister_node(node);
  }

  void handle_heartbeat_request (void* session_arg, std::string msg_content) {
  }

  void handle_core_stat_request (void* session_arg, std::string msg_content) {
    core_stat_receiver.request_handler(session_arg, msg_content); 
  }
private:
  HeartBeatService hb_worker;
  HDCSCoreStatController core_stat_receiver;
};

}// ha
}// hdcs

#endif
