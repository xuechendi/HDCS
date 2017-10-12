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
#include "common/C_AioRequestCompletion.h"



class stats
{
public:
    stats(int timeout) : timeout_(timeout)
    {
    }

    void add(bool error, size_t count_written, size_t count_read,
        size_t bytes_written, size_t bytes_read)
    {
        total_error_count_ += error? 1: 0;
        total_count_written_ += count_written;
        total_count_read_ += count_read;
        total_bytes_written_ += bytes_written;
        total_bytes_read_ += bytes_read;
    }

    void print()
    {
        std::cout << total_error_count_ << " total count error\n";
        std::cout << total_count_written_ << " total count written\n";
        std::cout << total_count_read_ << " total count read\n";
        std::cout << total_bytes_written_ << " total bytes written\n";
        std::cout << total_bytes_read_ << " total bytes read\n";
        std::cout << static_cast<double>(total_bytes_read_) /
            (timeout_ * 1024 * 1024) << " MiB/s read throughput\n";
        std::cout << static_cast<double>(total_bytes_written_) /
            (timeout_ * 1024 * 1024) << " MiB/s write throughput\n";
    }

private:
    size_t total_error_count_ = 0;
    size_t total_bytes_written_ = 0;
    size_t total_bytes_read_ = 0;
    size_t total_count_written_ = 0;
    size_t total_count_read_ = 0;
    int timeout_;
};

class session
{
public:
    session(boost::asio::io_service& ios, uint64_t block_size)
        : io_service_(ios),
        socket_(ios),
        block_size_(block_size),
        buffer_(new char[sizeof(void*)]) {
    }

    ~session() {
        delete[] buffer_;
    }

    void write(const char* data, uint64_t length, void* arg) {
        char* data_buffer = const_cast<char*>(data);
        uint64_t seq_id = (uint64_t)arg;
        memcpy(data_buffer, &seq_id, sizeof(uint64_t));
        //printf("client write, callback: %lX\n", seq_id);
        boost::asio::async_write(socket_, boost::asio::buffer(data_buffer, length),
            [this](const boost::system::error_code& err, uint64_t cb) {
            if (!err) {
                read();
            } else {
                if (!want_close_) {
                    //std::cout << "write failed: " << err.message() << "\n";
                    error_ = true;
                }
            }
        });
    }

    void read() {
        boost::asio::async_read(socket_, boost::asio::buffer(buffer_, sizeof(void*)),
            [this](const boost::system::error_code& err, uint64_t cb) {
            if (!err) {
                uint64_t seq_id = (*((uint64_t*)buffer_));
                hdcs::AioCompletion *comp = (hdcs::AioCompletion*)seq_id;
                //printf("client read, callback: %lX\n", seq_id);
                comp->complete(0);
            } else {
                if (!want_close_) {
                    //std::cout << "read failed: " << err.message() << "\n";
                    error_ = true;
                }
            }
        });
    }

    void start(boost::asio::ip::tcp::endpoint endpoint) {
        socket_.async_connect(endpoint, [this](const boost::system::error_code& err) {
            if (!err)
            {
                boost::asio::ip::tcp::no_delay no_delay(true);
                socket_.set_option(no_delay);
            }
        });
    }

    void stop() {
        io_service_.post([this]() {
            want_close_ = true;
            socket_.close();
        });
    }

    size_t bytes_written() const {
        return bytes_written_;
    }

    size_t bytes_read() const {
        return bytes_read_;
    }

    size_t count_written() const {
        return count_written_;
    }

    size_t count_read() const {
        return count_read_;
    }

    bool error() const {
        return error_;
    }
private:
    boost::asio::io_service& io_service_;
    boost::asio::ip::tcp::socket socket_;
    uint64_t block_size_;
    char* buffer_;
    size_t bytes_written_ = 0;
    size_t bytes_read_ = 0;
    size_t count_written_ = 0;
    size_t count_read_ = 0;
    bool error_ = false;
    bool want_close_ = false;
};

class client
{
public:
    client(int thread_count,
        char const* host, char const* port,
        uint64_t block_size, size_t session_count)
        : thread_count_(thread_count), s_id(0) {
        io_services_.resize(thread_count_);
        io_works_.resize(thread_count_);
        threads_.resize(thread_count_);

        for (auto i = 0; i < thread_count_; ++i) {
            io_services_[i].reset(new boost::asio::io_service);
            io_works_[i].reset(new boost::asio::io_service::work(*io_services_[i]));
            threads_[i].reset(new std::thread([this, i]() {
                auto& io_service = *io_services_[i];
                try {
                    io_service.run();
                } catch (std::exception& e) {
                    std::cerr << "Exception: " << e.what() << "\n";
                }
            }));
        }

        resolver_.reset(new boost::asio::ip::tcp::resolver(*io_services_[0]));
        boost::asio::ip::tcp::resolver::iterator iter =
            resolver_->resolve(boost::asio::ip::tcp::resolver::query(host, port));
        endpoint_ = *iter;

        for (size_t i = 0; i < session_count; ++i)
        {
            auto& io_service = *io_services_[i % thread_count_];
            std::unique_ptr<session> new_session(new session(io_service, block_size));
            sessions_.emplace_back(std::move(new_session));
        }

        start();
    }

    ~client() {
        for (auto& io_work : io_works_) {
            io_work.reset();
        }

        for (auto& session : sessions_) {
            session->stop();
        }

    }

    void start() {
        for (auto& session : sessions_) {
            session->start(endpoint_);
        }
    }

    void wait() {
        for (auto& thread : threads_) {
            thread->join();
        }
    }

    int send(const char* data, uint64_t length, void* arg) {
        sessions_[s_id++]->write(data, length, arg);
        if (s_id >= thread_count_) s_id = 0;
        return 0;
    }

private:
    std::atomic<int> s_id;
    int const thread_count_;
    std::vector<std::unique_ptr<boost::asio::io_service>> io_services_;
    std::vector<std::unique_ptr<boost::asio::io_service::work>> io_works_;
    std::unique_ptr<boost::asio::ip::tcp::resolver> resolver_;
    std::vector<std::unique_ptr<std::thread>> threads_;
    std::vector<std::unique_ptr<session>> sessions_;
    boost::asio::ip::tcp::endpoint endpoint_;
};



