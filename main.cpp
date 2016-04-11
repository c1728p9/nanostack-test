/*
 * Copyright (c) 2015 ARM Limited. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "mbed-client/m2minterfacefactory.h"
#include "mbed-client/m2mdevice.h"
#include "mbed-client/m2minterfaceobserver.h"
#include "mbed-client/m2minterface.h"
#include "mbed-client/m2mobjectinstance.h"
#include "mbed-client/m2mresource.h"
//#include "security.h"

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

//Select binding mode: UDP or TCP
M2MInterface::BindingMode SOCKET_MODE = M2MInterface::UDP;

// This is address to mbed Device Connector
#ifdef MESH
const String &MBED_SERVER_ADDRESS = "coap://[2607:f0d0:2601:52::20]:5684";
#else
const String &MBED_SERVER_ADDRESS = "coap://api.connector.mbed.com:5684";
#endif

const String &MBED_USER_NAME_DOMAIN = 0;//MBED_DOMAIN;
const String &ENDPOINT_NAME = 0;//MBED_ENDPOINT_NAME;

const String &MANUFACTURER = "manufacturer";
const String &TYPE = "type";
const String &MODEL_NUMBER = "2015";
const String &SERIAL_NUMBER = "12345";

const uint8_t STATIC_VALUE[] = "mbed-client";


#if defined(TARGET_K64F)
#define OBS_BUTTON SW2
#define UNREG_BUTTON SW3
#endif

// Set up Hardware interrupt button.
InterruptIn obs_button(SW2);
InterruptIn unreg_button(SW3);

// Network interaction must be performed outside of interrupt context
Semaphore updates(0);
volatile bool registered = false;
osThreadId mainThread;

void unregister() {
    registered = false;
    updates.release();
}

void update() {
    updates.release();
}

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

    if( cb_res->event_type == SOCKET_DATA )
    {

        //Read data from the RX buffer
        length = socket_read( cb_res->socket_id,
                &source_addr,
                buffer,
                sizeof(buffer) );
        buffer[31] = 0;

        if( length )
        {
//            if( cb_res->socket_id == app_udp_socket )
//            {
//                // Handles data received in UDP socket
//
//                sn_nsdl_process_coap(rx_buffer, length, &sn_addr_s);
//            }
            output.printf("Received message %s", buffer);
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

//    void * socket = network_interface->socket_create(NSAPI_UDP);
//    if (0 == socket) {
//    	output.printf("Error creating socket\r\n");
//    	while (true);
//    }
//
//
//    mesh->socket_attach_recv(socket, socket_data);
//    mesh->socket_bind(socket, 1234);


    int8_t socket = socket_open(SOCKET_UDP, 1234, socket_data);
    if (socket < 0) {
    	output.printf("Error opening socket: %i", socket);
    }
//    ns_address_t addr = {
//    		ADDRESS_IPV6,
//			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
//			1234
//
//    };
//    const uint8_t ns_in6addr_any[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
//    int8_t ret = socket_bind(socket, )


    while (true) {
        int update = updates.wait(25000);
    }

    status_ticker.detach();
}


