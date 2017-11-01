#include <algorithm>
#include <iostream>
#include <list>
#include <string>
#include <mutex>
#include <assert.h>
#include <vector>
#include <memory>
#include <queue>
#include <stack>
#include <thread>
#include <future>
#include "io_pool.h"
#include "Message.h"

namespace hdcs_server {
typedef std::function<void(void*, std::string)> callback_t;

//class session : public std::enable_shared_from_this<session>
class session {
public:
    session(boost::asio::io_service& ios, callback_t cb)
        : io_service_(ios)
        , socket_(ios)
        , cb(cb)  
        , read_worker(nullptr)
        , go(true)
        , buffer_(new char[sizeof(MsgHeader)]) {
    }

    ~session() {
      delete[] buffer_;
      //delete read_worker;
    }

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void read_cycle() {
      while(go) {
        read();
      }
    }

    void start() {
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay);
        if (read_worker == nullptr) {
          read_worker = new std::thread(std::bind(&session::read_cycle, this)); 
        }
    }

    void stop() {
      go = false;
      socket_.close();
    }

    void aio_write(std::string send_buffer) {
      Message msg(send_buffer);
      boost::asio::async_write(socket_, boost::asio::buffer(msg.to_buffer()),
          //[this, self = shared_from_this()](
          [this](
            const boost::system::error_code& err, size_t cb) {
            if (!err) {
            }
      });
    }

    void read() {
      uint64_t exact_read_bytes;
      try {
        exact_read_bytes = socket_.read_some(boost::asio::buffer(buffer_, sizeof(MsgHeader)));
      } catch (std::exception& e) {
        if (exact_read_bytes == 0) {
          std::cerr << "Exception: " << e.what() << "\n";
          stop();
          return;
        }
      }
        assert(exact_read_bytes == sizeof(MsgHeader));
        if (((MsgHeader*)buffer_)->msg_flag != 0xFFFFFFFFFFFFFFFF) {
          printf("%p received wrong msg\n", &socket_);
          for (int i = 0; i < sizeof(MsgHeader); i++) {
            if(i!=0 && i%32==0) printf("\n");
            if(i!=0 && i%8==0) printf("  ");
            if(i!=0 && i%2==0) printf(" ");
            printf("%02x", (unsigned char)buffer_[i]);
          }
          printf("\n");
          stop();
          return;
        }
        uint64_t content_size = ((MsgHeader*)buffer_)->get_data_size();
        if (content_size != 4144) {
          printf("expected data size %lu data\n", content_size);
        }
        char* data_buffer = (char*)malloc(content_size);
        uint64_t left = content_size;
        while (left > 0) {
          exact_read_bytes = socket_.read_some(boost::asio::buffer(&data_buffer[content_size - left], left));
          left = left - exact_read_bytes;
        }

        cb((void*)this, std::move(std::string(data_buffer, content_size)));
        free(data_buffer);
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    char* buffer_;
    callback_t cb;
    std::thread *read_worker;
    bool go;
};
}// server

class server
{
public:
  server(int thread_count, std::string host, std::string port)
        : thread_count_(thread_count)
        , service_pool_(thread_count)
        , acceptor_(service_pool_.get_io_service()) {
        auto endpoint = boost::asio::ip::tcp::endpoint(
          boost::asio::ip::address::from_string(host), stoi(port));
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(1));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

  ~server() {
    for (auto &session : sessions_) {
      delete session;
    }
  }

    void start(hdcs_server::callback_t task) {
        sessions_.emplace_back(new hdcs_server::session(
            service_pool_.get_io_service(), task));
        hdcs_server::session* new_session = *(sessions_.end() - 1);
        auto& socket = new_session->socket();
        acceptor_.async_accept(socket,
            [this, new_session, task](
                const boost::system::error_code& err) {
            if (!err) {
              printf("new connection: %p\n", new_session);
                new_session->start();
                start(task);
            }
        });
    }

    void wait() {
      service_pool_.run();
    }
    
    void send(void* session_id, std::string send_buffer) {
      ((hdcs_server::session*)session_id)->aio_write(send_buffer);
    }

private:
    int const thread_count_;
    io_service_pool service_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;    
    std::vector<hdcs_server::session*> sessions_;
};
