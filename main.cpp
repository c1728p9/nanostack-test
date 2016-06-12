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
#define MESH_LOWPAN_ND
#undef MESH_THREAD

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

// Status indication
Ticker status_ticker;
DigitalOut status_led(LED1);
DigitalOut command_led(LED2);
void blinky() { status_led = !status_led; }

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
            //output.printf("Socket Addr: %s\r\n", source_addr.get_ip_address());
            data = (uint8_t *)"Packet recieved\r\n";
            int ret = sock.sendto(source_addr, data, strlen((char*)data));
            //output.printf("Send returned %i\r\n", ret);
            if (strcmp((char*)buffer, "on") == 0) {
                command_led = 0;
            }
            if (strcmp((char*)buffer, "off") == 0) {
                command_led = 1;
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
                //output.printf("Connect returned %i\r\n", ret);
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

void tcp_main(const void *net)
{
    while (true) {
        osEvent event = tcp_main_queue.get();
        if (event.status != osEventMessage) {
            //output.printf("Invalid return %i\r\n", event.status);
            continue;
        }
        //output.printf("Socket recieved\r\n");

        TCPSocket *socket = (TCPSocket *)event.value.p;
        while (true) {
            uint8_t data[64];
            memset(data, 0, sizeof(data));
            int ret = socket->recv(data, sizeof(data));
            if (ret > 0) {
                //output.printf("Read data: %s\r\n", data);
            } else {
                //output.printf("Ret: %i\r\n", ret);
                break;
            }
            char resp[] = "Got message 2";
            ret = socket->send(resp, sizeof(resp));
            //output.printf("Send ret: %i\r\n", ret);
        }
        delete socket;
    }
}

// Entry point to the program
int main() {
    status_ticker.attach_us(blinky, 250000);

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
        output.printf("No IP address\r\n");
    }

    Thread udp_thread(udp_main, (void*)network_interface);
    Thread tcp_thread(tcp_main, (void*)network_interface);

    status_ticker.detach();

    while (true) {
        rtos::Thread::wait(1000);
    }
}
