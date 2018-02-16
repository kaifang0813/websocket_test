#include "uWS.h"
#include <iostream>
int main()
{
    uWS::Hub h;

    h.onMessage([](uWS::WebSocket<uWS::SERVER> *ws, char *message, size_t length, uWS::OpCode opCode) {
        
        //std::cout <<  message << std::endl;
        ws->send(message, length, opCode);
    });

    h.listen(9006);
    h.run();
}
