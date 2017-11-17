// Copyright [2017] <Intel>
#ifndef HDCS_CORE_H
#define HDCS_CORE_H

#include "common/WorkQueue.h"
#include "common/Request.h"
#include "common/Config.h"
#include "common/ThreadPool.h"
#include "common/tq.h"
#include "core/BlockRequest.h"
#include "core/BlockGuard.h"
#include "core/policy/Policy.h"
#include <mutex>
#include <map>
#include <string>

namespace hdcs {
namespace core {
  class HDCSCore {
  public:
    std::mutex core_lock;
    WorkQueue<std::shared_ptr<Request>> request_queue;
    HDCSCore(std::string name, std::string config_name);
    ~HDCSCore();
    void close();
    void promote_all();
    void flush_all();
    void queue_io (std::shared_ptr<Request> req);
    void aio_read (char* data, uint64_t offset, uint64_t length, void* c);
    void aio_write (char* data, uint64_t offset, uint64_t length, void* c);
  private:
    TWorkQueue *hdcs_op_threads;
    std::thread *main_thread;
    bool go;
    Config *config;
    Policy* policy;
    BlockGuard* block_guard;
    std::mutex block_request_list_lock;
    BlockRequestList block_request_list;
    //slave hdcs core list
    template<char delimiter>
    class WordDelimitedBy : public std::string {};
    std::mutex replication_core_map_mutex;
    hdcs_replica_nodes_t replication_core_map;
    bool cache_policy_mode;

    void process();
    void process_request(std::shared_ptr<Request> req);
    void map_block(BlockRequest &&block_request);
    void connect_to_replica(std::string name);
    void replica_send_out(AioCompletion* comp, uint64_t offset, uint64_t length, char* data);

  };
}// core
}// hdcs

#endif
