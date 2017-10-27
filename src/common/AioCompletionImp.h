#ifndef HDCS_AIOCOMPLETIONIMP_H
#define HDCS_AIOCOMPLETIONIMP_H

#include "AioCompletion.h"
#include <atomic>
#include <functional>

namespace hdcs{

class AioCompletionImp : public AioCompletion {
public:
  AioCompletionImp() : defined(false), Callback(nullptr), data(nullptr), shared_count(1) {}
  AioCompletionImp(std::function<void(ssize_t)>&& Callback, int shared_count = 1) : defined(true), Callback(Callback), data(nullptr), shared_count(shared_count) {}
  ~AioCompletionImp(){
    if (data != nullptr) {
      free(data);
    }
  }
  void complete(ssize_t r) {
    if (defined) {
      if (--shared_count == 0 && defined) {
        Callback(r);
        delete this;
      } else {
        return;
      }
    } else {
      delete this;
    }
  }
  ssize_t get_return_value() {};
  void wait_for_complete() {};
  void set_reserved_ptr(void* ptr) {
    data = (char*)ptr;
  }
  std::function<void(ssize_t)> Callback;
  char* data;
  bool defined;
  std::atomic<int> shared_count;
};

}// hdcs
#endif
