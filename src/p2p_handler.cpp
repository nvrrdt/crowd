// this one manages p2p behaviour
// always listening server
// unless polling the network as a client

#include "p2p_handler.hpp"
  
using namespace crowd;

void p2p_handler::p2p_switch()
{
    p2p_handler cl;
    cl.client("127.0.0.1");

    p2p_handler se;
    se.server_main();
}
