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

uint8_t buffer[32];
UDPSocket sock;
FunctionPointer fp(func);
SocketAddress source_addr;
uint8_t * data = NULL;
int length = 0;
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
    const char *ip_addr = network_interface->get_ip_address();
    if (ip_addr) {
        output.printf("IP address %s\r\n", ip_addr);
    } else {
        output.printf("No IP address %s\r\n");
    }


//    socket = socket_open(SOCKET_UDP, 1234, socket_data);
//    if (socket < 0) {
//    	output.printf("Error opening socket: %i", socket);
//    }

//    TCPSocket tcp;
//    tcp.open(network_interface);
//    SocketAddress tcp_addr("fd00:0ff1:ce0b:a5e0:04db:d10f:949e:99fd", 1234);
//    tcp.connect(tcp_addr);
//    tcp.

    int ret;
    TCPSocket tcp(network_interface);
    TCPServer server(network_interface);
    ret = server.bind(1234);
    output.printf("Bind: %i\r\n", ret);
    ret = server.listen(1);
    output.printf("listen: %i\r\n", ret);

    TCPSocket client(network_interface);
    ret = server.accept(&client);
    output.printf("accept: %i\r\n", ret);
    client.set_blocking(true);
    client.set_timeout(1000000);
    while (true) {
        uint8_t data[64];
        memset(data, 0, sizeof(data));
        int ret = client.recv(data, sizeof(data));
        if (ret > 0) {
            output.printf("Read data: %s\r\n", data);
        } else {
            output.printf("Ret: %i\r\n", ret);
        }
        char resp[] = "Got message";
        ret = client.send(resp, sizeof(resp));
        output.printf("Send ret: %i\r\n", ret);
    }





//    sock.open(network_interface);
//    sock.attach(fp);
//    sock.bind(1234);
//    while (true) {
//        memset(buffer, 0, sizeof(buffer));
//        length = sock.recvfrom(&source_addr, buffer, sizeof(buffer) - 1);
//        if (length > 0) {
//            output.printf("Socket Addr: %s\r\n", source_addr.get_ip_address());
//            data = (uint8_t *)"Packet recieved\r\n";
//            int ret = sock.sendto(source_addr, data, strlen((char*)data));
//            output.printf("Send returned %i\r\n", ret);
//            if (strcmp((char*)buffer, "on") == 0) {
//                command_led = 0;
//            }
//            if (strcmp((char*)buffer, "off") == 0) {
//                command_led = 1;
//            }
//        }
//    }


    while (true) {
        rtos::Thread::wait(1000);
    }

    status_ticker.detach();
}


