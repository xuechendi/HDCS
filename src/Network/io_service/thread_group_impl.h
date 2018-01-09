#ifndef THREAD_GROUP_IMPL_H_
#define THREAD_GROUP_IMPL_H_

#include <unistd.h>
#include <pthread.h>
#include <cstdio>
#include <memory>
#include <vector>

#include <boost/bind.hpp>
#include <boost/asio.hpp>

#include "../common/networking_common.h"

namespace hdcs {
namespace networking {

class ThreadGroupImpl;
typedef std::shared_ptr<ThreadGroupImpl> ThreadGroupImplPtr;

class ThreadGroupImpl{
public:
    ThreadGroupImpl(int thread_num)
        : _is_running(false)
        , _thread_num(std::max(thread_num, 1))
        , _io_service_work(NULL)
    {}

    ~ThreadGroupImpl(){
        stop();
    }

    int thread_num() const{
        return _thread_num;
    }

    IOService& io_service(){
        return _io_service;
    }

    bool start()
    {
        if (_is_running){
            return true;
        }
        _is_running = true;
        _io_service_work = new IOServiceWork(_io_service);

        for (int i = 0; i < _thread_num; ++i)
        {
            _threads.push_back(std::thread([this](){_io_service.run();}));
        }
        return true;
    }

    void stop(){
        if (!_is_running){
            return;
        }
        _is_running = false;

        delete _io_service_work;
        _io_service_work = NULL;

        for (int i = 0; i < _thread_num; ++i){
            // don't call io_service.stop() 
            _threads[i].join();
        }
    }

    // Request the thread group to invoke the given handler.
    // The handler may be executed inside this function if the guarantee can be met.
    // The function signature of the handler must be:
    //   void handler();
    template< typename CompletionHandler >
    void dispatch(CompletionHandler handler){
        _io_service.dispatch(handler);
    }

    // Request the thread group to invoke the given handler and return immediately.
    // It guarantees that the handle will not be called from inside this function.
    // The function signature of the handler must be:
    //   void handler();
    template< typename CompletionHandler >
    void post(CompletionHandler handler){
        _io_service.post(handler);
    }

private:
    volatile bool _is_running;
    int _thread_num;
    IOService _io_service;
    IOServiceWork* _io_service_work;
    std::vector<std::thread> _threads;
};

} 
} 

#endif 
