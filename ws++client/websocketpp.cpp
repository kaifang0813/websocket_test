




/*
 * Copyright (c) 2014, Peter Thorson. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the WebSocket++ Project nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL PETER THORSON BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// **NOTE:** This file is a snapshot of the WebSocket++ utility client tutorial.
// Additional related material can be found in the tutorials/utility_client
// directory of the WebSocket++ repository.

#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/client.hpp>

#include <websocketpp/common/thread.hpp>
#include <websocketpp/common/memory.hpp>
//#include <unistd.h>
#include <thread>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <chrono>
#include <vector>

auto my_start = std::chrono::high_resolution_clock::now();
int message_sent=0;
auto my_sendtime = std::chrono::high_resolution_clock::now();

auto my_receivetime = std::chrono::high_resolution_clock::now();
std::vector<long> time_vector;
std::vector<long> time_send;
std::vector<long> time_rece;



typedef websocketpp::client<websocketpp::config::asio_client> client;

class connection_metadata {
public:
    typedef websocketpp::lib::shared_ptr<connection_metadata> ptr;
    
    connection_metadata(int id, websocketpp::connection_hdl hdl, std::string uri)
    : m_id(id)
    , m_hdl(hdl)
    , m_status("Connecting")
    , m_uri(uri)
    , m_server("N/A")
    {}
    
    void on_open(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Open";
        
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("Server");
    }
    
    void on_fail(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Failed";
        
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        m_server = con->get_response_header("Server");
        m_error_reason = con->get_ec().message();
    }
    
    void on_close(client * c, websocketpp::connection_hdl hdl) {
        m_status = "Closed";
        client::connection_ptr con = c->get_con_from_hdl(hdl);
        std::stringstream s;
        s << "close code: " << con->get_remote_close_code() << " ("
        << websocketpp::close::status::get_string(con->get_remote_close_code())
        << "), close reason: " << con->get_remote_close_reason();
        m_error_reason = s.str();
    }
    
    void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
        my_receivetime = std::chrono::high_resolution_clock::now();
        //std:: cout << "message receieve" << message_sent<<std::endl;;
        time_rece.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(my_receivetime-my_start).count());
        if (msg->get_opcode() == websocketpp::frame::opcode::text) {
            m_messages.push_back(msg->get_payload());
        } else {
            m_messages.push_back(websocketpp::utility::to_hex(msg->get_payload()));
        }
        //std::cout <<" i am in on message"<<std::endl;
        //my_sendtime= std::chrono::high_resolution_clock::now();

    }
    websocketpp::connection_hdl get_hdl() const {
        return m_hdl;
    }
    
    int get_id() const {
        return m_id;
    }
    
    std::string get_status() const {
        return m_status;
    }
    
    void record_sent_message(std::string message) {
        m_messages.push_back(">> " + message);
    }
    
    friend std::ostream & operator<< (std::ostream & out, connection_metadata const & data);
private:
    int m_id;
    websocketpp::connection_hdl m_hdl;
    std::string m_status;
    std::string m_uri;
    std::string m_server;
    std::string m_error_reason;
    std::vector<std::string> m_messages;
};

std::ostream & operator<< (std::ostream & out, connection_metadata const & data) {
    out << "> URI: " << data.m_uri << "\n"
    << "> Status: " << data.m_status << "\n"
    << "> Remote Server: " << (data.m_server.empty() ? "None Specified" : data.m_server) << "\n"
    << "> Error/close reason: " << (data.m_error_reason.empty() ? "N/A" : data.m_error_reason) << "\n";
    out << "> Messages Processed: (" << data.m_messages.size() << ") \n";
    
    std::vector<std::string>::const_iterator it;
    for (it = data.m_messages.begin(); it != data.m_messages.end(); ++it) {
        out << *it << "\n";
    }
    
    return out;
}

class websocket_endpoint {
public:
    websocket_endpoint () : m_next_id(0) {
        m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
        m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
        
        m_endpoint.init_asio();
        m_endpoint.start_perpetual();
        
        m_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&client::run, &m_endpoint);
    }
    
    ~websocket_endpoint() {
        m_endpoint.stop_perpetual();
        
        for (con_list::const_iterator it = m_connection_list.begin(); it != m_connection_list.end(); ++it) {
            if (it->second->get_status() != "Open") {
                // Only close open connections
                continue;
            }
            
            std::cout << "> Closing connection " << it->second->get_id() << std::endl;
            
            websocketpp::lib::error_code ec;
            m_endpoint.close(it->second->get_hdl(), websocketpp::close::status::going_away, "", ec);
            if (ec) {
                std::cout << "> Error closing connection " << it->second->get_id() << ": "
                << ec.message() << std::endl;
            }
        }
        
        m_thread->join();
    }
    
    int connect(std::string const & uri) {
        websocketpp::lib::error_code ec;
        
        client::connection_ptr con = m_endpoint.get_connection(uri, ec);
        
        if (ec) {
            std::cout << "> Connect initialization error: " << ec.message() << std::endl;
            return -1;
        }
        
        int new_id = m_next_id++;
        connection_metadata::ptr metadata_ptr = websocketpp::lib::make_shared<connection_metadata>(new_id, con->get_handle(), uri);
        m_connection_list[new_id] = metadata_ptr;
        
        con->set_open_handler(websocketpp::lib::bind(
                                                     &connection_metadata::on_open,
                                                     metadata_ptr,
                                                     &m_endpoint,
                                                     websocketpp::lib::placeholders::_1
                                                     ));
        con->set_fail_handler(websocketpp::lib::bind(
                                                     &connection_metadata::on_fail,
                                                     metadata_ptr,
                                                     &m_endpoint,
                                                     websocketpp::lib::placeholders::_1
                                                     ));
        con->set_close_handler(websocketpp::lib::bind(
                                                      &connection_metadata::on_close,
                                                      metadata_ptr,
                                                      &m_endpoint,
                                                      websocketpp::lib::placeholders::_1
                                                      ));
        con->set_message_handler(websocketpp::lib::bind(
                                                        &connection_metadata::on_message,
                                                        metadata_ptr,
                                                        websocketpp::lib::placeholders::_1,
                                                        websocketpp::lib::placeholders::_2
                                                        ));
        
        m_endpoint.connect(con);
        
        return new_id;
    }
    
    void close(int id, websocketpp::close::status::value code, std::string reason) {
        websocketpp::lib::error_code ec;
        
        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            std::cout << "> No connection found with id " << id << std::endl;
            return;
        }
        
        m_endpoint.close(metadata_it->second->get_hdl(), code, reason, ec);
        if (ec) {
            std::cout << "> Error initiating close: " << ec.message() << std::endl;
        }
    }
    
    void send(int id, std::string message) {
        websocketpp::lib::error_code ec;
        
        con_list::iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            std::cout << "> No connection found with id " << id << std::endl;
            return;
        }
        
        m_endpoint.send(metadata_it->second->get_hdl(), message, websocketpp::frame::opcode::text, ec);
        if (ec) {
            std::cout << "> Error sending message: " << ec.message() << std::endl;
            return;
        }
        
        metadata_it->second->record_sent_message(message);
    }
    
    connection_metadata::ptr get_metadata(int id) const {
        con_list::const_iterator metadata_it = m_connection_list.find(id);
        if (metadata_it == m_connection_list.end()) {
            return connection_metadata::ptr();
        } else {
            return metadata_it->second;
        }
    }
private:
    typedef std::map<int,connection_metadata::ptr> con_list;
    
    client m_endpoint;
    websocketpp::lib::shared_ptr<websocketpp::lib::thread> m_thread;
    
    con_list m_connection_list;
    int m_next_id;
};

int main() {
    bool done = false;
    std::string input;
    websocket_endpoint endpoint;
    
    
        std::cout << "Enter Command: ";
        std::getline(std::cin, input);
        
        
    
            int id=-1;
    
            if(input=="uws"){
                id = endpoint.connect("ws://127.0.0.1:9006");
            }else if(input=="wspp"){
                id = endpoint.connect("ws://127.0.0.1:9005");
            }else{
                id = endpoint.connect("ws://127.0.0.1:8085/echo");
            }
             
            if (id != -1) {
                std::cout << "> Created connection with id " << id << std::endl;
            }
            
    
             
             sleep(2);
             
            connection_metadata::ptr metadata = endpoint.get_metadata(0);
            if (metadata) {
                std::cout << *metadata << std::endl;
            } else {
                std::cout << "> Unknown connection id " << id << std::endl;
            }

             sleep(2);
        
            std::stringstream ss("hell0");
            
            std::string cmd;
            std::string message;
            
            ss >> cmd >> id;
            std::getline(ss,message);
            for(int i=0; i<10000; i++){
                //unsigned int microseconds = 100;
                
                //usleep(microseconds);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                my_sendtime = std::chrono::high_resolution_clock::now();
                endpoint.send(id, message);
                message_sent++;
                //std::cout << "message send" << message_sent <<std::endl;
                time_send.push_back(long(std::chrono::duration_cast<std::chrono::nanoseconds>(my_sendtime-my_start).count()));

            }
    
    sleep(10);
    std::stringstream ss3("close");
    
    std::string cmd3;
    int close_code = websocketpp::close::status::normal;
    std::string reason;
    
    ss3 >> cmd3 >> id >> close_code;
    std::getline(ss3,reason);
    
    endpoint.close(id, close_code, reason);
    
    
    for(int i=0; i<time_send.size()-1; i++){
        time_vector.push_back(time_rece[i]-time_send[i]);
       // std::cout << time_vector[i] <<std::endl;
    }
    
    
    
    std::cout << "send message" << time_send.size() << ", receive message: " << time_rece.size() << std::endl;
    double sum = std::accumulate(time_vector.begin(), time_vector.end(), 0.0);
    double mean = sum / time_vector.size();
    
    double sq_sum = std::inner_product(time_vector.begin(), time_vector.end(), time_vector.begin(), 0.0);
    double stdev = std::sqrt(sq_sum / time_vector.size() - mean * mean);
    
    std::cout << "the average time is: "<< mean <<std::endl;
    std::cout << "stdev: "<< stdev<<std::endl;

    std::cout << "the max time is: "<<time_vector[distance(time_vector.begin(),std::max_element(time_vector.begin(), time_vector.end()))] <<std::endl;;
    std::cout << "the min time is: "<<time_vector[distance(time_vector.begin(),std::min_element(time_vector.begin(), time_vector.end()))] <<std::endl;;

    
    
    return 0;
}

