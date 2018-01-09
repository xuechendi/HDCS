#ifndef DS_SERVER 
#define DS_SERVER

#include <sstream>
#include <map>
#include <vector>
#include <thread>
#include <atomic>
#include "./common/session.h"
#include "./common/networking_common.h"
#include "acceptor.h"

namespace hdcs{
namespace networking{

class server{
private:
    SessionSet session_set; 
    std::unique_ptr<Acceptor> acceptor_ptr;
    std::atomic<bool> is_stop;

public:

    server(std::string _ip_address, std::string _port_num, int s_num=10, int thd_num=10): 
        is_stop(false){
        acceptor_ptr.reset(new Acceptor( _ip_address, _port_num, session_set , s_num, thd_num));
    }

    ~server(){
        stop();
    }

    void stop(){
        if(is_stop.load()){
            return;
        }
        is_stop.store(true);
        for(auto it=session_set.begin(); it!=session_set.end(); ++it){
            (*it)->stop(); 
            session_set.erase(it);
        }
        acceptor_ptr->stop();
    }

    void sync_run(){
        acceptor_ptr->sync_run();
    }

    void async_run(){
        acceptor_ptr->async_run();
    }

    // start listen 
    bool start( ProcessMsg process_msg ){
        acceptor_ptr->start( process_msg );
    }

    //this is async send, but hdcs use this interface name.
    void send(void* session_arg, std::string send_buffer, OnSentServer _callback=NULL){
        async_send(session_arg, send_buffer);   
    }

    void async_send(void* session_arg, std::string& send_buffer ){
        Session* temp_s_id = (Session*)(((SessionArg*)session_arg)->get_session_id());
        if(session_set.find(temp_s_id)==session_set.end()){
            std::cout<<"Networking::server: finding session_id failed. Maybe need to re-connction "<<std::endl;
            assert(0);
        }
        temp_s_id->async_send(send_buffer, ((SessionArg*)session_arg)->get_seq_id());
    }
}; 
}
}
#endif
/*
    asio::error::operation_aborted  -1
    asio::error::connection_aborted -2
    asio::error::connection_reset -3
    asio::error::bad_descriptor -4
    asio::error::interrupted -5
    asio::error::network_down -6
    asio::error::not_connected -7
    asio::error::shut_down -8

*/
