#ifndef HDCS_CLIENT_H
#define HDCS_CLIENT_H
#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <mutex>
#include <assert.h>
#include <vector>
#include <memory>
#include <thread>
#include <boost/asio/steady_timer.hpp>
#include "io_pool.h"
#include "Network/Message.h"
#include "common/Timer.h"
#include "common/AioCompletionImp.h"

namespace client {
typedef std::function<void(void*, std::string)> callback_t;

class session
{
public:
    session(boost::asio::io_service& ios, callback_t cb)
        : io_service_(ios),
        socket_(ios),
        counts(0),
        cb(cb),
        batch_event(nullptr),
        read_worker(nullptr),
        buffer_(new char[sizeof(MsgHeader)]) {}

    ~session() {
      cv.notify_all();
      printf("delete session %p\n", this);
      delete[] buffer_;
      printf("delete session %p completed\n", this);
    }

    void set_arg(void* cb_arg) {
      arg = cb_arg;
      printf("set arg for session %p, arg: %p\n", this, arg);
    }

    void aio_write(std::string send_buffer) {
      boost::asio::async_write(socket_, boost::asio::buffer(std::move(send_buffer)),
        [this](const boost::system::error_code& err, uint64_t cb) {
        if (!err) {
        } else {
        }
      });
    }

    void write(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::write(socket_,  boost::asio::buffer(std::move(msg.to_buffer())));
      counts++;
    }

    void aio_communicate(std::string send_buffer) {
      counts++;
      if (read_worker == nullptr) {
        read_worker = new std::thread(std::bind(&session::read_cycle, this)); 
      }
      cv.notify_all();
      Message msg(send_buffer);
      // insert into send_buffer firstly, and sent out in 0.1ms
      //batch_mutex.lock();
      batch_mutex.lock();
      cache_buffer.append(std::move(msg.to_buffer()));
      if (batch_event != nullptr) {
        if (cache_buffer.size() >= 65536) {
          hdcs::AioCompletion* tmp = batch_event;
          aio_write(cache_buffer);
          batch_event = nullptr;
          cache_buffer.clear();
          batch_mutex.unlock();

          batch_send_timer.cancel_event(tmp);
        } else {
          batch_mutex.unlock();
        }
      } else {
        batch_event = new hdcs::AioCompletionImp([&](ssize_t r){
          batch_mutex.lock();    
          aio_write(cache_buffer);
          batch_event = nullptr;
          cache_buffer.clear();
          batch_mutex.unlock();    
        });
        batch_mutex.unlock();
        batch_send_timer.add_event_after(100, batch_event);
      } 
    }

    void aio_read() {
      boost::asio::async_read(socket_, boost::asio::buffer(buffer_, sizeof(MsgHeader)),
        [this](const boost::system::error_code& err, uint64_t exact_read_bytes) {
        if (!err) {
          counts--;
          assert(exact_read_bytes == sizeof(MsgHeader));
          uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
          //printf("client about to receive ack, size is %lu\n", content_size);
          char* data_buffer = (char*)malloc(content_size);
          exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
          if (exact_read_bytes == content_size) {
            cb(arg, std::move(std::string(data_buffer, content_size))); 
            free(data_buffer);
          } else {
            printf("client received bytes : %lu, less than expected %lu bytes\n", exact_read_bytes, content_size);
          }
        } else {
        }
      });
    }

    ssize_t read() {
      //printf("client session %p about to receive header\n", this);
      ssize_t exact_read_bytes = socket_.read_some(boost::asio::buffer(buffer_, sizeof(MsgHeader)));
      ssize_t ret = 0;
      counts--;
      assert(exact_read_bytes == sizeof(MsgHeader));
      uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
      //printf("client session %p about to receive ack, size is %lu\n", this, content_size);
      char* data_buffer = (char*)malloc(content_size);
      exact_read_bytes = socket_.read_some(boost::asio::buffer(data_buffer, content_size));
      //printf("client session %p received content\n", this);
      if (exact_read_bytes == content_size) {
        cb(arg, std::move(std::string(data_buffer, content_size))); 
        free(data_buffer);
      }
    }

    void read_cycle() {
      std::unique_lock<std::mutex> l(read_worker_mutex);
      while(counts > 0) {
        read();
        if (counts == 0) cv.wait(l);
      }
    }

    void start(boost::asio::ip::tcp::endpoint endpoint) {
      try {
        socket_.connect(endpoint);
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay);
      } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << "\n";
      }
    }

    void stop() {
        uint64_t tmp;
        int retry = 0;
        while(counts > 0 && retry < 1){
          tmp = counts;
          printf("session %p still has %lu inflight ios\n", this, tmp);
          sleep(1);
          retry++;
        }
        cv.notify_all();
        io_service_.post([this]() {
            socket_.close();
            printf("session %p closed\n", this);
        });
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    char* buffer_;
    std::atomic<uint64_t> counts;
    void* arg;
    callback_t cb;

    std::condition_variable cv;
    std::string cache_buffer;
    std::thread *read_worker;
    std::mutex batch_mutex;
    std::mutex read_worker_mutex;
    hdcs::SafeTimer batch_send_timer;
    hdcs::AioCompletion* batch_event;
};

}// client
class Connection {
public:
    Connection(client::callback_t task)
        : thread_count_(1), s_id(0), session_count(1),
        task(task) {
        io_services_.resize(thread_count_);
        io_works_.resize(thread_count_);
        threads_.resize(thread_count_);

        for (auto i = 0; i < thread_count_; ++i) {
            io_services_[i].reset(new boost::asio::io_service);
            io_works_[i].reset(new boost::asio::io_service::work(*io_services_[i]));
            threads_[i].reset(new std::thread([this, i]() {
                auto& io_service = *io_services_[i];
                //io_service.run();
                try {
                    io_service.run();
                } catch (std::exception& e) {
                    std::cerr << "Exception: " << e.what() << "\n";
                    close();
                }
            }));
        }

        resolver_.reset(new boost::asio::ip::tcp::resolver(*io_services_[0]));
    }

    ~Connection() {
      printf("delete connection\n");
        for (auto& io_work : io_works_) {
            io_work.reset();
        }
      printf("delete connection completed\n");
    }

    void close() {
        for (auto& session : sessions_) {
          session->stop();
        }
        delete this;
    }

    void connect( std::string host, std::string port) {
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_->resolve(boost::asio::ip::tcp::resolver::query(host, port));
        endpoint_ = *iter;
        start();
    }

    void start() {
        for (size_t i = 0; i < session_count; ++i)
        {
            auto& io_service = *io_services_[i % thread_count_];
            std::unique_ptr<client::session> new_session(new client::session(io_service, task));
            sessions_.emplace_back(std::move(new_session));
        }
        for (auto& session : sessions_) {
            session->start(endpoint_);
        }
    }

    void wait() {
        for (auto& thread : threads_) {
            thread->join();
        }
    }

    void set_session_arg(void* arg) {
        for (auto& session : sessions_) {
            session->set_arg(arg);
        }
    }

    int aio_communicate(std::string send_buffer) {
        sessions_[s_id]->aio_communicate(send_buffer);
        if (++s_id >= session_count) s_id = 0;
        return 0;
    }

    int communicate(std::string send_buffer) {
      ssize_t ret = 0;
      sessions_[s_id]->write(send_buffer);
      sessions_[s_id]->read();
      if (++s_id >= session_count) s_id = 0;
      return 0;
    }

private:
    std::atomic<int> s_id;
    int const thread_count_;
    int const session_count;
    std::vector<std::unique_ptr<boost::asio::io_service>> io_services_;
    std::vector<std::unique_ptr<boost::asio::io_service::work>> io_works_;
    std::unique_ptr<boost::asio::ip::tcp::resolver> resolver_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::vector<std::unique_ptr<client::session>> sessions_;
    boost::asio::ip::tcp::endpoint endpoint_;
    client::callback_t task;
};
#endif
