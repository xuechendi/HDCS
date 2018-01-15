#ifndef HDCS_COMMON_HEARTBEAT_SERVICE_H
#define HDCS_COMMON_HEARTBEAT_SERVICE_H

#include "Network/hdcs_networking.h"
#include "common/Timer.h"
#include "common/HeartBeat/HeartBeat.h"

namespace hdcs {
 
typedef std::map<std::string, std::shared_ptr<HeartBeat>> hdcs_heartbeat_map_t;

class HeartBeatService {
public:
  HeartBeatService (HeartBeatOpts &&hb_opts) :
    hb_opts(hb_opts),
    event_timer(std::make_shared<SafeTimer>()) {
  }

  ~HeartBeatService () {
  }

  int add_listener (std::string port) {
    hb_server = std::make_shared<networking::server>("0.0.0.0", port, 1, 1);
    hb_server->start([&](void* p, std::string s){request_handler(p, s, hb_server.get());});
    hb_server->sync_run();
    return 0;
  }

  int register_node (networking::Connection* conn, std::string node, std::shared_ptr<AioCompletion> error_handler) {
    if (!conn) {
      int colon_pos;
      colon_pos = node.find(':');
      std::string addr = node.substr(0, colon_pos);
      std::string port = node.substr(colon_pos + 1, node.length() - colon_pos);
      conn = new networking::Connection([&](void* p, std::string s){request_handler(p, s);}, 1, 1);
      conn->connect(addr, port);
    }
  
    hb_map[node] = std::make_shared<HeartBeat>(conn, error_handler, &hb_opts, event_timer);
    return 0;
  }

  int unregister_node (std::string node) {
    //remove events in timer?
    //remove node in map
    hb_map.erase(node);
    return 0;
  }

  void request_handler(void* session_id, std::string s, networking::server* listener = nullptr) {
    //switch msg_type
    HDCS_HEARTBEAT_MSG_T* hb_msg = (HDCS_HEARTBEAT_MSG_T*)s.c_str();
    switch (hb_msg->type) {
      case HDCS_HEARTBEAT_REPLY:
        ((HeartBeat*)(hb_msg->hb_inst_id))->request_handler(session_id, s);
        break;
      case HDCS_HEARTBEAT_PING:
        HDCS_HEARTBEAT_MSG msg_content(HDCS_HEARTBEAT_REPLY, hb_msg->hb_inst_id);
        listener->send(session_id, std::move(std::string(msg_content.data(), msg_content.size())));
        break;
    }
  }

private:
  hdcs_heartbeat_map_t hb_map;
  std::shared_ptr<SafeTimer> event_timer;
  HeartBeatOpts hb_opts;
  std::shared_ptr<networking::server> hb_server;
};
}

#endif
