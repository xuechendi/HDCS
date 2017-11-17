// Copyright [2017] <Intel>
#include "include/libhdcs.h"
#include "common/C_AioRequestCompletion.h"
#include "core/HDCSCore.h"

void hdcs_aio_release(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  delete comp;
}

void hdcs_aio_wait_for_complete(hdcs_completion_t c){
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  comp->wait_for_complete();
}

int hdcs_aio_create_completion(void *cb_arg, callback_t complete_cb, hdcs_completion_t *c){
  hdcs::AioCompletion *comp = new hdcs::C_AioRequestCompletion(cb_arg, complete_cb);
  *c = (hdcs_completion_t) comp;
  return 0;
}

ssize_t hdcs_aio_get_return_value(hdcs_completion_t c) {
  hdcs::AioCompletion *comp = (hdcs::C_AioRequestCompletion*) c;
  return comp->get_return_value();
}

int hdcs_open(void** io, char* name) {
  *io = new hdcs::core::HDCSCore(name, "general.conf");
  return 0;
}

int hdcs_close(void* io) {
  ((hdcs::core::HDCSCore*)io)->close();
  delete (hdcs::core::HDCSCore*)io;
  return 0;
}

int hdcs_aio_read(void* io, char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* arg = (void*)c;
  ((hdcs::core::HDCSCore*)io)->aio_read(data, offset, length, arg);
  return 0;
}

int hdcs_aio_write(void* io, const char* data, uint64_t offset, uint64_t length, hdcs_completion_t c){
  void* arg = (void*)c;
  ((hdcs::core::HDCSCore*)io)->aio_write(const_cast<char*>(data), offset, length, arg);
  return 0;
}

int hdcs_promote_all(void* io) {
  ((hdcs::core::HDCSCore*)io)->promote_all();
  return 0;
}

int hdcs_flush_all(void* io) {
  ((hdcs::core::HDCSCore*)io)->flush_all();
  return 0;
}
