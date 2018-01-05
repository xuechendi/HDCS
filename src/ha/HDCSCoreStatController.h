#ifndef HDCS_CORE_STAT_CONTROLLER_H
#define HDCS_CORE_STAT_CONTROLLER_H

#include "Network/hdcs_networking.h"
#include "common/Timer.h"
#include "ha/HDCSCoreStat.h"

namespace hdcs {

namespace ha {
 
typedef std::map<void*, std::shared_ptr<HDCSCoreStat>> hdcs_core_stat_map_t;

typedef uint8_t HDCS_HA_MSG_TYPE;
#define HDCS_HA_CORE_STAT_UPDATE  0XD2

class HDCS_CORE_STAT_MSG {
public:
  HDCS_CORE_STAT_MSG (uint8_t cores_num)
    : cores_num(cores_num),
      data_size(sizeof(HDCS_CORE_STAT_T) * cores_num) {
    data_ = (char*)malloc (data_size);
  }

  HDCS_CORE_STAT_MSG (std::string msg) {
    data_size = msg.length();
    data_ = (char*)malloc (data_size);
    memcpy(data_, msg.c_str(), data_size);
    cores_num = data_size / sizeof(HDCS_CORE_STAT_T);
  }

  ~HDCS_CORE_STAT_MSG() {
    free(data_);
  }

  uint8_t get_cores_num() {
    return cores_num;
  }

  char* data() {
    return data_;
  }

  uint64_t size() {
    return data_size + sizeof(cores_num);
  }

  void loadline(uint8_t index, HDCS_CORE_STAT_T *stat) {
    memcpy(((HDCS_CORE_STAT_T*)data_)+index, stat, sizeof(HDCS_CORE_STAT_T));
  } 

  void readline(uint8_t index, HDCS_CORE_STAT_T *stat) {
    memcpy(stat, ((HDCS_CORE_STAT_T*)data_)+index, sizeof(HDCS_CORE_STAT_T));
  } 

private:
  char* data_;
  uint8_t cores_num;
  uint64_t data_size;
};

class HDCSCoreStatController {
public:
  HDCSCoreStatController():conn(nullptr) {
  }

  HDCSCoreStatController(std::string addr, std::string port) {
    std::cout << "addr:" << addr << " port:" << port << std::endl;
    conn = new networking::Connection([&](void* p, std::string s){receiver_handler(p, s);}, 1, 1);
    conn->connect(addr, port);
  }

  ~HDCSCoreStatController() {
    if (conn)
      conn->close();
  }

  int add_listener (std::string port) {
    core_stat_listener = new networking::server("0.0.0.0", port, 1, 1);
    core_stat_listener->start([&](void* p, std::string s){request_handler(p, s);});
    core_stat_listener->run();
    return 0;
  }

  std::shared_ptr<HDCSCoreStat> register_core (void* hdcs_core_id) {
    std::shared_ptr<AioCompletion> error_handler = std::make_shared<AioCompletionImp>([&](ssize_t r){
      send_stat_map();
      printf("one core stat is updated, send out new stat map\n");
    }, -1);
    auto it = hdcs_core_stat_map.insert(
      std::pair<void*, std::shared_ptr<HDCSCoreStat>>(
        hdcs_core_id,
        std::make_shared<HDCSCoreStat>(hdcs_core_id, error_handler)
      )
    );
    return it.first->second;
  }

  int unregister_core (void* hdcs_core_id) {
    hdcs_core_stat_map.erase(hdcs_core_id);
    return 0;
  }

  void send_stat_map() {
    HDCS_CORE_STAT_MSG msg_content(hdcs_core_stat_map.size());
    uint8_t i = 0;
    for (auto &item : hdcs_core_stat_map) {
      msg_content.loadline(i++, item.second->get_stat());
    }
    conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  }

  void request_handler(void* session_id, std::string msg_content) {
    HDCS_CORE_STAT_MSG core_stat_msg(msg_content);
    HDCS_CORE_STAT_T tmp_stat;
    for (uint8_t i = 0; i < core_stat_msg.get_cores_num(); i++) {
      core_stat_msg.readline(i, &tmp_stat);
      ha_hdcs_core_stat[tmp_stat.hdcs_core_id] = tmp_stat.stat;
    }

    for (auto &print_stat : ha_hdcs_core_stat) {
      printf("new update    ");
      printf("%x", print_stat.first);
      /*uint64_t tmp = (uint64_t)(print_stat.first);
      for (int i = 0; i < sizeof(print_stat.first); i++){
        printf("%x", ((char*)(&tmp))[i]);
      }*/
      printf(": %x\n", print_stat.second);
    }
  }

  void receiver_handler(void* session_id, std::string msg_content) {

  }

private:
  hdcs_core_stat_map_t hdcs_core_stat_map;
  networking::Connection* conn;
  networking::server* core_stat_listener;
  std::map<void*, HDCS_CORE_STAT_TYPE> ha_hdcs_core_stat; 

};
}// ha
}// hdcs
#endif
