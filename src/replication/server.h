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
#include "io_pool.h"


class session : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::io_service& ios, size_t block_size)
        : io_service_(ios)
        , socket_(ios)
        , block_size_(block_size)
        , buffer_(new char[block_size]) {
    }

    ~session() {
        delete[] buffer_;
    }

    boost::asio::ip::tcp::socket& socket() {
        return socket_;
    }

    void start() {
        boost::asio::ip::tcp::no_delay no_delay(true);
        socket_.set_option(no_delay);
        read();
    }

    void write() {
        boost::asio::async_write(socket_, boost::asio::buffer(buffer_, sizeof(void*)),
            [this, self = shared_from_this()](
                const boost::system::error_code& err, size_t cb) {
            if (!err) {
                assert(cb == block_size_);
                read();
            }
        });
    }

    void read() {
        boost::asio::async_read(socket_, boost::asio::buffer(buffer_, block_size_),
            [this, self = shared_from_this()](
                const boost::system::error_code& err, size_t cb) {
            if (!err) {
                assert(cb == block_size_);
                write();
            }
        });
    }

private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    size_t const block_size_;
    char* buffer_;
};

class server
{
public:
    server(int thread_count, const boost::asio::ip::tcp::endpoint& endpoint,
        size_t block_size)
        : thread_count_(thread_count)
        , block_size_(block_size)
        , service_pool_(thread_count)
        , acceptor_(service_pool_.get_io_service())
    {
        acceptor_.open(endpoint.protocol());
        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(1));
        acceptor_.bind(endpoint);
        acceptor_.listen();
    }

    void start() {
        accept();
    }

    void wait() {
        service_pool_.run();
    }
private:
    void accept() {
        std::shared_ptr<session> new_session(new session(
            service_pool_.get_io_service(), block_size_));
        auto& socket = new_session->socket();
        acceptor_.async_accept(socket,
            [this, new_session = std::move(new_session)](
                const boost::system::error_code& err) {
            if (!err) {
                new_session->start();
                accept();
            }
        });
    }

private:
    int const thread_count_;
    size_t const block_size_;
    io_service_pool service_pool_;
    boost::asio::ip::tcp::acceptor acceptor_;
};
