#include<stdint.h>

#include "mbed.h"
#include "rtos.h"

#include "net_interface.h"
#include "socket_api.h"

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

osThreadId mainThread;

int8_t socket = 0;

// Status indication
Ticker status_ticker;
DigitalOut status_led(LED1);
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
            output.printf("Received message %s\r\n", buffer);
            uint8_t * data = (uint8_t *)"Packer recieved\r\n";
            socket_sendto(cb_res->socket_id, &source_addr, data, strlen((char*)data));
        }
    }

}

// Entry point to the program
int main() {
    status_ticker.attach_us(blinky, 250000);

    // Keep track of the main thread
    mainThread = osThreadGetId();

    // Sets the console baud-rate
    output.baud(115200);

    output.printf("Starting Nanostack example\r\n");

    NetworkInterface *network_interface = 0;
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

    socket = socket_open(SOCKET_UDP, 1234, socket_data);
    if (socket < 0) {
    	output.printf("Error opening socket: %i", socket);
    }

    while (true) {
        rtos::Thread::wait(1000);
    }

    status_ticker.detach();
}


