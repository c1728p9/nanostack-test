#include<stdint.h>

#include "mbed.h"
#include "rtos.h"

#include "net_interface.h"
#include "socket_api.h"
#include "UDPSocket.h"
#include "TCPSocket.h"
#include "TCPServer.h"

#undef ETHERNET
#undef WIFI
#undef MESH_LOWPAN_ND
#define MESH_THREAD

#if defined WIFI
#include "ESP8266Interface.h"
ESP8266Interface esp(D1, D0);
#elif defined (ETHERNET)
#include "LWIPInterface.h"
LWIPInterface lwip;
#elif defined (MESH_LOWPAN_ND)
#define MESH
#include "NanostackInterface.h"
LoWPANNDInterface mesh;
#elif defined (MESH_THREAD)
#define MESH
#include "NanostackInterface.h"
ThreadInterface mesh;
#endif

#include "ns_sal_utils.h"

Serial output(USBTX, USBRX);

osThreadId mainThread;

int8_t socket = 0;

// Status indication
Ticker status_ticker;
DigitalOut status_led(LED1);
DigitalOut command_led(LED2);
void blinky() { status_led = !status_led; }

void socket_data(void * cb)
{
    ns_address_t source_addr;
    uint8_t buffer[32];
	output.printf("Packet!\r\n");
    socket_callback_t *cb_res = (socket_callback_t *)cb;
    int16_t length;

    if( cb_res->socket_id != socket ) {
        // This isn't our socket
        output.printf("Message for wrong socket: %i\r\n", cb_res->socket_id);
        return;
    }

    if( cb_res->event_type == SOCKET_DATA ) {
        //Read data from the RX buffer
        memset(buffer, 0, sizeof(buffer));
        length = socket_read( cb_res->socket_id,
                &source_addr,
                buffer,
                sizeof(buffer) - 1);

        if (length) {
            output.printf("Received message \"%s\"\r\n", buffer);
            uint8_t * data = (uint8_t *)"Packet recieved\r\n";
            socket_sendto(cb_res->socket_id, &source_addr, data, strlen((char*)data));
            if (strcmp((char*)buffer, "on") == 0) {
                command_led = 0;
            }
            if (strcmp((char*)buffer, "off") == 0) {
                command_led = 1;
            }



        }
    }

}

void func(void);


//UDPSocket sock;
FunctionPointer fp(func);

void func(void)
{
//    memset(buffer, 0, sizeof(buffer));
//    length = sock.recvfrom(&source_addr, buffer, sizeof(buffer) - 1);
//    if (length > 0) {
//        data = (uint8_t *)"Packet recieved\r\n";
//        sock.sendto(source_addr, data, strlen((char*)data));
//        if (strcmp((char*)buffer, "on") == 0) {
//            command_led = 0;
//        }
//        if (strcmp((char*)buffer, "off") == 0) {
//            command_led = 1;
//        }
//
//    }
}

static TCPSocket *tcp_ptr = NULL;
static Queue<TCPSocket, 16> tcp_main_queue;

void udp_main(const void *net)
{
    NetworkStack *network = (NetworkStack*)net;

    UDPSocket sock(network);
    SocketAddress source_addr;
    uint8_t buffer[32];
    uint8_t * data = NULL;
    int length = 0;

    //sock.attach(fp);
    sock.set_blocking(true);
    sock.bind(1234);
    TCPSocket *local_ptr = NULL;
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        length = sock.recvfrom(&source_addr, buffer, sizeof(buffer) - 1);
        if (length > 0) {
            output.printf("Socket Addr: %s\r\n", source_addr.get_ip_address());
            data = (uint8_t *)"Packet recieved\r\n";
            int ret = sock.sendto(source_addr, data, strlen((char*)data));
            output.printf("Send returned %i\r\n", ret);
            if (strcmp((char*)buffer, "on") == 0) {
                command_led = 0;
            }
            if (strcmp((char*)buffer, "off") == 0) {
                command_led = 1;
            }
            if (strcmp((char*)buffer, "reset") == 0) {
                if (tcp_ptr != NULL) {
                    tcp_ptr->close();
                }
            }
            if (strcmp((char*)buffer, "close") == 0) {
                if (local_ptr != NULL) {
                    local_ptr->close();
                    local_ptr = NULL;
                }
            }
            uint16_t port = 0;
            if (1 == sscanf((char*)buffer, "connect %hu", &port)) {
                if (local_ptr != NULL) {
                    local_ptr->close();
                }
                SocketAddress addr(source_addr);
                addr.set_port(port);
                local_ptr = new TCPSocket(network);
                ret = local_ptr->connect(addr);
                output.printf("Connect returned %i\r\n", ret);
                if (0 == ret) {
                    tcp_main_queue.put(local_ptr);
                } else {
                    delete local_ptr;
                    local_ptr = NULL;
                }
            }
        }
    }
}
//tcp_main_queue.put()
void tcp_main(const void *net)
{
    while (true) {
        osEvent event = tcp_main_queue.get();
        if (event.status != osEventMessage) {
            output.printf("Invalid return %i\r\n", event.status);
            continue;
        }
        output.printf("Socket recieved\r\n");

        TCPSocket *socket = (TCPSocket *)event.value.p;
        while (true) {
            uint8_t data[64];
            memset(data, 0, sizeof(data));
            int ret = socket->recv(data, sizeof(data));
            if (ret > 0) {
                output.printf("Read data: %s\r\n", data);
            } else {
                output.printf("Ret: %i\r\n", ret);
                break;
            }
            char resp[] = "Got message 2";
            ret = socket->send(resp, sizeof(resp));
            output.printf("Send ret: %i\r\n", ret);
        }
    }
}

//void socket_callback(void *cb) {
//    socket_callback_t *sock_cb = (socket_callback_t *) cb;
//    output.printf("socket_callback() sock=%d, event=%d, interface=%d, data len=%d\r\n",
//                     sock_cb->socket_id, sock_cb->event_type, sock_cb->interface_id, sock_cb->d_len);
//    ns_address_t ns_address;
//    if (SOCKET_BIND_DONE == sock_cb->event_type) {
//        SocketAddress addr;
//        int ret = socket_read_session_address(sock_cb->socket_id, &ns_address);
//        output.printf("Session info: %i\r\n", ret);
//        convert_ns_addr_to_mbed(&addr, &ns_address);
//        output.printf("IP: %s, port %i\r\n", addr.get_ip_address(), addr.get_port());
//    }
//
//    if (sock_cb->event_type != SOCKET_DATA) {
//        return;
//    }
//    uint8_t buffer[100];
//    memset(buffer, 0, sizeof(buffer));
//    int16_t length = socket_read(sock_cb->socket_id, &ns_address, buffer, sizeof(buffer));
//    if (length >= 0) {
//        SocketAddress addr;
//        convert_ns_addr_to_mbed(&addr, &ns_address);
//        output.printf("IP: %s, port %i\r\n", addr.get_ip_address(), addr.get_port());
//        buffer[99] = 0;
//        output.printf("Data: %s\r\n", buffer);
//        if (strcmp((char*)buffer, "close") == 0) {
//            int8_t resp = socket_close(sock_cb->socket_id, &ns_address);
//            output.printf("socket_close: %i\r\n", resp);
//        } else {
//            int8_t resp = socket_sendto(sock_cb->socket_id, &ns_address, buffer, length);
//            output.printf("socket_sendto: %i\r\n", resp);
//        }
//    }
//
//}
extern "C" void eventOS_scheduler_mutex_wait(void);
extern "C" void eventOS_scheduler_mutex_release(void);
// Entry point to the program
int main() {
    status_ticker.attach_us(blinky, 250000);

    // Keep track of the main thread
    mainThread = osThreadGetId();

    // Sets the console baud-rate
    output.baud(115200);

    output.printf("Starting Nanostack example\r\n");

    NetworkStack *network_interface = 0;
#if defined WIFI
    output.printf("\n\rUsing WiFi \r\n");
    output.printf("\n\rConnecting to WiFi..\r\n");
    #error "Remember to provide your WiFi credentials and provide in esp.connect("ssid","password");"
    esp.connect("ssid", "password");
    output.printf("\n\rConnected to WiFi\r\n");
    network_interface = &esp;
#elif defined ETHERNET
    lwip.connect();
    output.printf("Using Ethernet LWIP\r\n");
    network_interface = &lwip;
#elif defined MESH
    mesh.connect();
    network_interface = &mesh;
#endif
    //eventOS_scheduler_mutex_wait();

    const char *ip_addr = network_interface->get_ip_address();
    if (ip_addr) {
        output.printf("IP address %s\r\n", ip_addr);
    } else {
        output.printf("No IP address\r\n");
    }


    /**
     * \brief Release the event loop mutex
     *
     * Release the mutex claimed with eventOS_scheduler_mutex_wait(),
     * allowing the event loop to continue processing.
     */

//    int8_t socket = socket_open(SOCKET_TCP, 0, socket_callback);
//    output.printf("socket_open ret %i\r\n", socket);
//
//    //SocketAddress addr("fd00:ff1:ce0b:a5e0:fcc2:3d00:4:ea8c", NSAPI_IPv6, 1234);
//    SocketAddress addr("0000:0000:0000:0000:0000:0000:0000:0000", NSAPI_IPv6, 1234);
//    ns_address_t ns_addr;
//    convert_mbed_addr_to_ns(&ns_addr, &addr);
////Event check list
//    int8_t ret = socket_bind(socket, &ns_addr);
//    output.printf("socket_bind ret %i\r\n", ret);
//
//    ret = socket_listen(socket);
//    output.printf("socket_listen ret %i\r\n", ret);
//
//    eventOS_scheduler_mutex_release();

//    socket = socket_open(SOCKET_UDP, 1234, socket_data);
//    if (socket < 0) {
//    	output.printf("Error opening socket: %i", socket);
//    }

//    TCPSocket tcp;
//    tcp.open(network_interface);
//    SocketAddress tcp_addr("fd00:0ff1:ce0b:a5e0:04db:d10f:949e:99fd", 1234);
//    tcp.connect(tcp_addr);
//    tcp.



    Thread udp_thread(udp_main, (void*)network_interface);
    Thread tcp_thread(tcp_main, (void*)network_interface);


    int ret;
    TCPServer server(network_interface);
    server.set_blocking(true);
    ret = server.bind(1234);
    output.printf("Bind: %i\r\n", ret);
    ret = server.listen(1);
    output.printf("listen: %i\r\n", ret);
    while (true) {
        TCPSocket client(network_interface);
        ret = server.accept(&client);
        output.printf("accept: %i\r\n", ret);
        //server.close();
        client.set_blocking(true);
        tcp_ptr = &client;
        while (true) {
            uint8_t data[64];
            memset(data, 0, sizeof(data));
            int ret = client.recv(data, sizeof(data));
            if (ret > 0) {
                output.printf("Read data: %s\r\n", data);
            } else {
                output.printf("Ret: %i\r\n", ret);
                break;
            }
            char resp[] = "Got message";
            ret = client.send(resp, sizeof(resp));
            output.printf("Send ret: %i\r\n", ret);
        }
        output.printf("Closing socket: %i\r\n", ret);
        tcp_ptr = NULL;
        client.close();
        //Thread::wait(1000000);
    }








    while (true) {
        rtos::Thread::wait(1000);
    }

    status_ticker.detach();
}


