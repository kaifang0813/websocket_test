#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;
using websocketpp::lib::bind;

// pull out the type of messages sent by our config
typedef server::message_ptr message_ptr;

// Define a callback to handle incoming messages
void on_message(server* s, websocketpp::connection_hdl hdl, message_ptr msg) {
    //std::cout << "get a message " <<std::endl;
    
    try {
        s->send(hdl, msg->get_payload(), msg->get_opcode());
    } catch (const websocketpp::lib::error_code& e) {

    }
}

int main() {
    // Create a server endpoint
    server echo_server;

    // 100mb max (used in huge throughput test)
    echo_server.set_max_message_size(1024 * 1024 * 100);

    try {
        // Set logging settings
        echo_server.set_access_channels(websocketpp::log::alevel::none);
        echo_server.clear_access_channels(websocketpp::log::alevel::frame_payload);

        // Initialize Asio
        echo_server.init_asio();

        // Register our message handler
        echo_server.set_message_handler(bind(&on_message,&echo_server,::_1,::_2));
        boost::asio::ip::tcp::endpoint ep1(boost::asio::ip::address::from_string("127.0.0.1"), 9001);
        websocketpp::lib::error_code ec;

        // Listen on port 9002
        
       
        echo_server.listen(9005);

        websocketpp::lib::asio::error_code error;
        auto endpoint = echo_server.get_local_endpoint(error);
        if (error) {
            std::cerr<<"error getting local endpoint";
        }
        std::cout<<endpoint.address()<<"\t"<<endpoint.port()<<std::endl;
                // Start the server accept loop
        echo_server.start_accept();
        
        
        // Start the ASIO io_service run loop
        echo_server.run();
    } catch (websocketpp::exception const & e) {
        std::cout << e.what() << std::endl;
    } catch (...) {
        std::cout << "other exception" << std::endl;
    }
}
