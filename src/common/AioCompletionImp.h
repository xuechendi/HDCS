#ifndef HDCS_AIOCOMPLETIONIMP_H
#define HDCS_AIOCOMPLETIONIMP_H

#include "AioCompletion.h"
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace hdcs{

class AioCompletionImp : public AioCompletion {
public:
  AioCompletionImp() : defined(false), Callback(nullptr), data(nullptr), shared_count(1) {
  }
  AioCompletionImp(std::function<void(ssize_t)>&& Callback, int shared_count = 1) : defined(true), Callback(Callback), data(nullptr), shared_count(shared_count) {}
  ~AioCompletionImp(){
    if (data != nullptr) {
      free(data);
    }
  }
  void complete(ssize_t r) {
    if (defined) {
      if (--shared_count == 0) {
        cond.notify_all();
        Callback(r);
      }
    }
  }
  ssize_t get_return_value() {};
  void wait_for_complete() {
    if (shared_count > 0) {
      std::unique_lock<std::mutex> l(cond_lock);
      cond.wait_for(l, std::chrono::milliseconds(50), [&]{return (shared_count == 0);});
    }
  };
  void set_reserved_ptr(void* ptr) {
    data = (char*)ptr;
  }
  std::function<void(ssize_t)> Callback;
  char* data;
  bool defined;
  std::atomic<int> shared_count;
  std::mutex cond_lock;
  std::condition_variable cond;
};

}// hdcs
#endif
