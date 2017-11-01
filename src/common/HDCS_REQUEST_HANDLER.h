// Copyright [2017] <Intel>
#ifndef HDCS_REQUEST_HANDLER_H
#define HDCS_REQUEST_HANDLER_H

#include "Network/client.h"
#include "common/HDCS_REQUEST_CTX.h"

struct hdcs_ioctx_t{
  Connection* conn;
  void* hdcs_inst; 
};

namespace client {

void request_handler(void* io, std::string msg_content) {
  hdcs::HDCS_REQUEST_CTX_T *io_ctx =  (hdcs::HDCS_REQUEST_CTX_T*)(msg_content.c_str());
    char* data = &msg_content[sizeof(hdcs::HDCS_REQUEST_CTX_T)];
    switch (io_ctx->type) {
      case HDCS_CONNECT_REPLY:
        ((hdcs_ioctx_t*)io)->hdcs_inst = io_ctx->hdcs_inst;
        break;
      case HDCS_READ_REPLY:
        if((ssize_t)(io_ctx->ret_data_ptr) != -1) {
          memcpy(io_ctx->ret_data_ptr, data, io_ctx->length);
          ((hdcs::AioCompletion*)io_ctx->comp)->complete(0);
        } else {
          ((hdcs::AioCompletion*)io_ctx->comp)->complete(-1);
        }
        break;
      case HDCS_WRITE_REPLY:
        //fprintf(stderr, "HDCS WRITE REQ COMPLETED: %p\n", io_ctx->comp);
        ((hdcs::AioCompletion*)io_ctx->comp)->complete((ssize_t)io_ctx->ret_data_ptr);
        break;
      case HDCS_FLUSH_REPLY:
        break;
      case HDCS_PROMOTE_REPLY:
        break;
      case HDCS_SET_CONFIG_REPLY:
        break;
      case HDCS_GET_STATUS_REPLY:
        break;
      default:
        break;
    }
}

}//client
#endif
