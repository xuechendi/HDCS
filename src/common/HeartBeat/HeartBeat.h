#ifndef HDCS_COMMON_HEARTBEAT_H
#define HDCS_COMMON_HEARTBEAT_H
#include <stdexcept>
#include <chrono>             // std::chrono::seconds
#include <mutex>              // std::mutex, std::unique_lock
#include <condition_variable> // std::condition_variable, std::cv_status
#include "common/AioCompletionImp.h"

namespace hdcs {

typedef uint8_t HeartBeatStat;
#define HEARTBEAT_STAT_OK      0XE0
#define HEARTBEAT_STAT_TIMEOUT 0XE1
typedef uint8_t HDCS_HA_MSG_TYPE;
#define HDCS_MSG_HEARTBEAT 0X31

struct HeartBeatOpts {
  uint64_t timeout_nanoseconds;
  uint64_t heartbeat_interval_nanoseconds;
  HeartBeatOpts(uint64_t timeout_nanoseconds, uint64_t heartbeat_interval_nanoseconds):
    timeout_nanoseconds(timeout_nanoseconds),
    heartbeat_interval_nanoseconds(heartbeat_interval_nanoseconds) {}
};

typedef uint8_t HEARTBEAT_MSG_TYPE;
struct HDCS_HEARTBEAT_MSG_T {
  HDCS_HA_MSG_TYPE reserved_flag;
  HEARTBEAT_MSG_TYPE type;
  void* hb_inst_id;
};
#define HDCS_HEARTBEAT_PING  0XE2
#define HDCS_HEARTBEAT_REPLY 0XE3
class HDCS_HEARTBEAT_MSG {
public:
  HDCS_HEARTBEAT_MSG (HEARTBEAT_MSG_TYPE type, void* id) {
    data_.reserved_flag = HDCS_MSG_HEARTBEAT;
    data_.type = type;
    data_.hb_inst_id = id;
  }

  ~HDCS_HEARTBEAT_MSG() {
  }

  char* data() {
    return (char*)&data_;
  }

  uint64_t size() {
    return sizeof(HDCS_HEARTBEAT_MSG_T);
  }

  HDCS_HEARTBEAT_MSG_T data_;
};

class HeartBeat {

private:

  HeartBeatOpts *hb_opts;
  std::shared_ptr<SafeTimer> event_timer;
  networking::Connection* conn;
  HeartBeatStat stat;
  AioCompletion* timeout_event;
  std::shared_ptr<AioCompletion> error_handler;
  std::string hb_msg;
  //std::mutex stat_mutex;

public:

  HeartBeat(networking::Connection* conn,
            std::shared_ptr<AioCompletion> error_handler,
            HeartBeatOpts *hb_opts,
            std::shared_ptr<SafeTimer> event_timer):
            conn(conn),
            hb_opts(hb_opts),
            event_timer(event_timer),
            stat(HEARTBEAT_STAT_OK),
            timeout_event(nullptr),
            error_handler(error_handler) {
    heartbeat_ping();
  }

  ~HeartBeat() {
    event_timer->cancel_event(timeout_event);
  }

  void request_handler(void* args, std::string reply_data) {
    //check msg
    // remove timeout then at a new timer event
    if (timeout_event) {
      event_timer->cancel_event(timeout_event);
    }
    heartbeat_ping();
  }

  void heartbeat_ping() {
    timeout_event = new AioCompletionImp([&](ssize_t r){
      // send out HeartBeat Msg
      HDCS_HEARTBEAT_MSG msg_content(HDCS_HEARTBEAT_PING, this);
      conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
      //printf("Heartbeat Msg sent out.\n");
    });
    event_timer->add_event_after(hb_opts->heartbeat_interval_nanoseconds, timeout_event);
      // add a timeout check event
      timeout_event = new AioCompletionImp([&](ssize_t r){
        //when this event happens, means HeartBeat timeout
        stat = HEARTBEAT_STAT_TIMEOUT;
        //printf("Heartbeat Msg no reply, timeout.\n");
        //notify others
        error_handler->complete(0);
      });
      event_timer->add_event_after(hb_opts->timeout_nanoseconds + hb_opts->heartbeat_interval_nanoseconds, timeout_event);
  }
};
}
#endif
