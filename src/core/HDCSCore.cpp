// Copyright [2017] <Intel>

#include "core/HDCSCore.h"

#include "core/policy/CachePolicy.h"
#include "core/policy/TierPolicy.h"
#include "store/SimpleStore/SimpleBlockStore.h"
#include "store/RBD/RBDImageStore.h"

#include <string>
#include "common/HDCS_REQUEST_HANDLER.h"

namespace hdcs {
namespace core {
HDCSCore::HDCSCore(std::string name, std::string cfg_file, struct hdcs_repl_options replication_options) {
  config = new Config(name, replication_options, cfg_file);

  std::string log_path = config->configValues["log_to_file"];
  std::cout << "log_path: " << log_path << std::endl;
  if( log_path!="false" ){
    int stderr_no = dup(fileno(stderr));
    log_fd = fopen( log_path.c_str(), "w" );
	  if(log_fd==NULL){}
    if(-1==dup2(fileno(log_fd), STDERR_FILENO)){}
  }

  int hdcs_thread_max = stoi(config->configValues["op_threads_num"]);
  for (int i = 0; i < 4; i++) {
    hdcs_op_threads[i] = new TWorkQueue( hdcs_thread_max );
  }
  uint64_t total_size = stoull(config->configValues["total_size"]);
  uint64_t block_size = stoull(config->configValues["cache_min_alloc_size"]);
  bool cache_policy_mode = config->configValues["policy_mode"].compare(std::string("cache")) == 0 ? true : false;

  std::string path = config->configValues["cache_dir_run"];
  std::string pool_name = config->configValues["rbd_pool_name"];
  std::string volume_name = name;

  //connect to its replication_nodes
  if ("hdcs_master" == replication_options.role && replication_options.replication_nodes != "") {
    connect_to_replica(name);
  }

  block_guard = new BlockGuard(total_size, block_size,
                               replication_core_map.size(),
                               std::move(replication_core_map));
  BlockMap* block_ptr_map = block_guard->get_block_map();

  if (cache_policy_mode) {
    uint64_t cache_size = stoull(config->configValues["cache_total_size"]);
    float cache_ratio_health = stof(config->configValues["cache_ratio_health"]);
    uint64_t timeout_nanosecond = stoull(config->configValues["cache_dirty_timeout_nanoseconds"]);
    CACHE_MODE_TYPE cache_mode = config->configValues["cache_mode"].compare(std::string("readonly")) == 0 ? CACHE_MODE_READ_ONLY : CACHE_MODE_WRITE_BACK;
    policy = new CachePolicy(total_size, cache_size, block_size, block_ptr_map,
                      new store::SimpleBlockStore(path, total_size, cache_size, block_size),
                      new store::RBDImageStore(pool_name, volume_name, block_size),
                      cache_ratio_health, &request_queue,
                      timeout_nanosecond, cache_mode, hdcs_thread_max);
  } else {
    policy = new TierPolicy(total_size, block_size, block_ptr_map,
                    new store::SimpleBlockStore(path, total_size, total_size, block_size),
                    new store::RBDImageStore(pool_name, volume_name, block_size),
                    &request_queue, hdcs_thread_max);
  }

  go = true;
  main_thread = new std::thread(std::bind(&HDCSCore::process, this));

}

HDCSCore::~HDCSCore() {
  go = false;
  main_thread->join();
  delete[] hdcs_op_threads;
  for (auto& hdcs_replica : replication_core_map) {
    ((hdcs_ioctx_t*)hdcs_replica.second)->conn->close();
    free((hdcs_ioctx_t*)hdcs_replica.second);
  }
  delete policy;
  delete block_guard;
  main_thread->join();
  delete main_thread;
}

void HDCSCore::close() {
#if defined(CACHE_POLICY)
  policy->flush_all();
#endif
}

void HDCSCore::promote_all() {
#if defined(TIER_POLICY)
  ((TierPolicy*)policy)->promote_all();
#endif
}

void HDCSCore::flush_all() {
#if defined(TIER_POLICY)
  ((TierPolicy*)policy)->flush_all();
#endif
}

void HDCSCore::queue_io(std::shared_ptr<Request> req) {
  //request_queue.enqueue((void*)req);
  process_request(req);
}

void HDCSCore::process() {
  while(go){
    std::shared_ptr<Request> req = request_queue.dequeue();
    if (req != nullptr) {
      //TODO: add TS;
      if (req->offset == 871018496) {
      struct timespec spec;
      clock_gettime(CLOCK_REALTIME, &spec);
      fprintf(stderr, "%lu: hdcs dequeue %lu - %lu\n", (spec.tv_sec * 1000000000L + spec.tv_nsec), req->offset, req->offset + req->length);
      }
      process_request(req);
      //TODO: add TS;
      if (req->offset == 871018496) {
      struct timespec spec;
      clock_gettime(CLOCK_REALTIME, &spec);
      fprintf(stderr, "%lu: hdcs complete process_request %lu - %lu\n", (spec.tv_sec * 1000000000L + spec.tv_nsec), req->offset, req->offset + req->length);
      }
    }
  }
}

void HDCSCore::process_request(std::shared_ptr<Request> req) {
  //std::mutex block_request_list_lock;
  //BlockRequestList block_request_list;
  std::lock_guard<std::mutex> lock(block_request_list_lock);
  block_guard->create_block_requests(req, &block_request_list);

  for (BlockRequestList::iterator it = block_request_list.begin(); it != block_request_list.end();) { 
    if (!it->block->in_discard_process) {
      map_block(std::move(*it));
      block_request_list.erase(it++);
    } else {
      it++;
    }
  }
}

void HDCSCore::map_block(BlockRequest &&block_request) {
  BlockOp *block_ops_end;
  Block* block = block_request.block;
  bool do_process = false;
  block->block_mutex.lock();
  // If this block_request belongs to discard block,
  // append to wait list firstly.
  BlockOp *block_ops_head = policy->map(std::move(block_request), &block_ops_end);
  if (!block->in_process) {
    block->in_process = true;
    block->block_ops_end = block_ops_end;
    do_process = true;
    log_print("Block not in process, block: %lu, new end: %p", block->block_id, block_ops_end);
  } else {
    log_print("Block in process, append request, block: %lu, append BlockOp: %p after BlockOp: %p, new end: %p", block->block_id, block_ops_head, block->block_ops_end, block_ops_end);
    block->block_ops_end->block_op_next = block_ops_head;
    block->block_ops_end = block_ops_end;
  }
  block->block_mutex.unlock();
  if (do_process) {
    uint8_t tp_index = block->block_id/(block_guard->get_block_count()/4);
    //printf("tp_index is %u\n", tp_index);
    hdcs_op_threads[tp_index]->add_task(std::bind(&BlockOp::send, block_ops_head, nullptr));
    //block_ops_head->send();
  }
}

void HDCSCore::aio_read (char* data, uint64_t offset, uint64_t length,  void* arg) {
  std::shared_ptr<Request> req = std::make_shared<Request>(IO_TYPE_READ, data, offset, length, arg);
  request_queue.enqueue(req);
  //process_request(req);
}

void HDCSCore::aio_write (char* data, uint64_t offset, uint64_t length,  void* arg) {
  std::shared_ptr<Request> req = std::make_shared<Request>(IO_TYPE_WRITE, data, offset, length, arg);
  //TODO: add TS;
  request_queue.enqueue(req);
  //process_request(req);
}

void HDCSCore::connect_to_replica (std::string name) {
  std::string addr;
  std::string port;
  int colon_pos, last_pos;
  char c;

  hdcs_ioctx_t* io_ctx;
  std::istringstream iss(config->configValues["replication_nodes"]);
  std::vector<std::string> replication_nodes((std::istream_iterator<WordDelimitedBy<','>>(iss)),
                                       std::istream_iterator<WordDelimitedBy<','>>());
  int replication_nodes_num = replication_nodes.size();
  for (auto &addr_port_str : replication_nodes) {
    colon_pos = addr_port_str.find(':');
    addr = addr_port_str.substr(0, colon_pos);
    c = addr_port_str.at(addr_port_str.length() - 1);

    if (c == ',') last_pos = 2;
    else last_pos = 1;
    port = addr_port_str.substr(colon_pos + 1, addr_port_str.length() - colon_pos - last_pos);
    std::cout << "Connect to replication_node: " << addr << " : " << port << std::endl;

    io_ctx = (hdcs_ioctx_t*)malloc(sizeof(hdcs_ioctx_t));
    replication_core_map[addr_port_str] = (void*)io_ctx;
    io_ctx->conn = new hdcs::networking::Connection([](void* p, std::string s){client::request_handler(p, s);}, 16, 5);
    io_ctx->conn->connect(addr, port);
    io_ctx->conn->set_session_arg((void*)io_ctx);

    hdcs::HDCS_REQUEST_CTX msg_content(HDCS_CONNECT, nullptr, nullptr, 0, name.length(), const_cast<char*>(name.c_str()));
    io_ctx->conn->communicate(std::move(std::string(msg_content.data(), msg_content.size())));
  }
}

}// core
}// hdcs
