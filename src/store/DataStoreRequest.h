// Copyright [2017] <Intel>
#ifndef HDCS_DATA_STORE_REQUEST_H
#define HDCS_DATA_STORE_REQUEST_H
#include <stdint.h>
#include <map>
#include <mutex>
#include "common/AioCompletionImp.h"
#include "store/DataStore.h"
#include "common/HDCS_REQUEST_CTX.h"

namespace hdcs {
namespace store {
typedef std::map<std::string, void*> hdcs_replica_nodes_t;
typedef std::map<uint64_t, std::pair<uint64_t, char*> > data_store_request_chain_t; 
//typedef void DataStore;

  class DataStoreRequest {
  public:
    DataStoreRequest(uint8_t shared_count, uint8_t replica_size,
                     uint64_t block_size, hdcs_replica_nodes_t* connection_v):
      shared_count(shared_count), data_store(nullptr),
      block_size(block_size), connection_v(connection_v){
        //create comp for replication request
        replica_write_comp = new AioCompletionImp([](ssize_t r){}, replica_size + 1);

        //create comp for prepare_data_store_req.
        data_store_comp = std::make_shared<AioCompletionImp>([&](ssize_t r){
          submit_request();
        }, shared_count, false);
    }

    ~DataStoreRequest() {
    }

    int prepare_request(DataStore* data_store_tmp, uint64_t block_id, char* data) {
      data_store_mutex.lock();
      data_store = data_store_tmp;

      //printf("offset: %lu, data: %p\n", block_id * block_size, data);
      data_store_request_chain_t::iterator cur_it;
      bool inserted = false;
      uint64_t block_size_tmp = block_size;
      uint8_t step = block_size_tmp / block_size;
      for (data_store_request_chain_t::iterator it = data_store_req_chain.begin(); 
          it != data_store_req_chain.end();) {
        cur_it = it++;

        // merge to the block before to this one
          if (cur_it->second.second + cur_it->second.first == data) {
            cur_it->second.first += block_size_tmp;
            block_id = cur_it->first;
            block_size_tmp = cur_it->second.first;
            data = cur_it->second.second;
            inserted = true;
          }

        // merge to the block next to this one
          if (data + block_size_tmp == cur_it->second.second) {
            uint64_t tmp_size = block_size_tmp + cur_it->second.first;
            //insert new one
            data_store_req_chain[block_id] = std::make_pair(tmp_size, data);
 
            //remove original one
            data_store_req_chain.erase(cur_it);
            block_size_tmp = tmp_size;
            inserted = true;
          }

      }// end for
      if (!inserted) {
        data_store_req_chain[block_id] = std::make_pair(block_size_tmp, data);
      }

      data_store_mutex.unlock();
      data_store_comp->complete(0);
      data_store_comp->wait_for_complete();
    }

    int submit_request() {
      void* io_ctx;
      int ret = 0;
      uint64_t offset;
      uint64_t length;
      char* data_ptr;
      //char print_buffer[256];
      //int print_buffer_size = 0;

      //sprintf(print_buffer, "New batch\n");
      for (data_store_request_chain_t::iterator it = data_store_req_chain.begin(); 
          it != data_store_req_chain.end(); it++) {

        //todo: different at cache mode.
        offset = it->first * block_size;
        length = it->second.first;
        data_ptr = it->second.second;

        //submit request to replica
        for (const auto& replica_node : *connection_v) {
          io_ctx = replica_node.second;
          hdcs::HDCS_REQUEST_CTX msg_content(HDCS_WRITE, ((hdcs_ioctx_t*)io_ctx)->hdcs_inst,
                                             replica_write_comp, offset, length, data_ptr);
          //((hdcs_ioctx_t*)io_ctx)->conn->aio_communicate(std::move(std::string(msg_content.data(), msg_content.size())));
        }

        //submit request to local
        //print_buffer_size = strlen(print_buffer);
        //sprintf(&print_buffer[print_buffer_size], "  DataStore %p write %p to %lu(%lu)\n", data_store, data_ptr, offset, length);
        ret = data_store->write(data_ptr, offset, length);
        if (ret < 0) {
          replica_write_comp->complete(ret);
        }
      }
      //printf("%s", print_buffer);
      replica_write_comp->complete(ret);
    }

  private:
    //DataStore
    data_store_request_chain_t data_store_req_chain;

    std::mutex data_store_mutex;
    std::shared_ptr<AioCompletion> data_store_comp;
    AioCompletion* replica_write_comp;
    uint8_t shared_count;
    DataStore* data_store;
    IO_TYPE io_type;
    uint64_t block_size;
    hdcs_replica_nodes_t* connection_v;

  };
}// store
}// hdcs

#endif
