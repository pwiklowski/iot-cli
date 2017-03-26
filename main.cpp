#include <iostream>
#include <unistd.h>

#include <string>
#include <poll.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/socket.h>

#include "COAPPacket.h"
#include "COAPServer.h"
#include "OICClient.h"
#include "OICDeviceResource.h"


using namespace std;

OICClient* oic_server=0;
List<OICDevice*> m_devices;
int m_socketFd;
pthread_t m_thread;



void showHelp(){
    cout << "h, help - show help" << endl;
    cout << "s, scan - show available devices" << endl;
    cout << "q, quit - close CLI" << endl;
}
OICDevice* getDevice(String di){

    for (uint8_t i=0; i<m_devices.size(); i++){
        OICDevice* d = m_devices.at(i);
        if (d->getId() == di){
            return d;
        }
    }
    return 0;
}

OICDeviceResource* getDeviceResource(OICDevice* dev, String href){

    for (uint8_t i=0; i<dev->getResources()->size(); i++){
        OICDeviceResource* v = dev->getResources()->at(i);
        if (v->getHref() == href){
            return v;
        }
    }
    return 0;
}

bool isDeviceOnList(String id){
    return getDevice(id) != 0;
}


void scan(){
    oic_server->searchDevices([&](COAPPacket* packet){
        cbor message;
        cbor::parse(&message, packet->getPayload());

        for (uint16_t i=0; i<message.toArray()->size(); i++){
            cbor device = message.toArray()->at(i);

            String name = device.getMapValue("n").toString();
            String di= device.getMapValue("di").toString();


            if (isDeviceOnList(di)) continue;

            cbor links = device.getMapValue("links");
            OICDevice* dev = new OICDevice(di, name, packet->getAddress(), oic_server);


            for (uint16_t j=0; j< links.toArray()->size(); j++){
                cbor link = links.toArray()->at(j);


                String href = link.getMapValue("href").toString();
                String rt = link.getMapValue("rt").toString();
                String iff = link.getMapValue("if").toString();

                dev->getResources()->push_back(new OICDeviceResource(href, iff, rt, dev, oic_server));
            }
            m_devices.append(dev);
        }
    });
    usleep(1000*500);

    cout << "Devices:" << endl;

    for (unsigned int i=0; i<m_devices.size(); i++){
        cout << "[" << i <<"] " << m_devices.at(i)->getId().c_str() << " - " << m_devices.at(i)->getName().c_str() << endl;
    }
}


String convertAddress(sockaddr_in a){
    char addr[30];
    sprintf(addr, "%d.%d.%d.%d %d",
            (uint8_t) (a.sin_addr.s_addr),
            (uint8_t) (a.sin_addr.s_addr >> 8),
            (uint8_t) (a.sin_addr.s_addr >> 16 ),
            (uint8_t) (a.sin_addr.s_addr >> 24),
            htons(a.sin_port));

    return addr;
}

int readPacket(uint8_t* buf, uint16_t maxSize, String* address){
    struct pollfd pfd;
    int res = sizeof(*buf);

    pfd.fd = m_socketFd;
    pfd.events = POLLIN;

    struct sockaddr_in client;
    socklen_t l = sizeof(client);
    size_t rc = poll(&pfd, 1, 20); // 1000 ms timeout
    if (rc >0){
        rc = recvfrom(m_socketFd, buf, maxSize, 0, (struct sockaddr *)&client,&l);

        *address = convertAddress(client);
    }
    return rc;
}

void* run(void* param){
    const int on = 1;
    m_socketFd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);


    struct sockaddr_in serv,client;
    struct ip_mreq mreq;

    serv.sin_family = AF_INET;
    serv.sin_port = 0;
    serv.sin_addr.s_addr = htonl(INADDR_ANY);

    uint8_t buffer[1024];
    socklen_t l = sizeof(client);
    if(setsockopt(m_socketFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        return 0;
    }
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 1000000;

    if( bind(m_socketFd, (struct sockaddr*)&serv, sizeof(serv) ) == -1)
    {
        return 0;
    }

    struct pollfd pfd;
    int res;

    pfd.fd = m_socketFd;
    pfd.events = POLLIN;
    String address;

    uint64_t lastTick = get_current_ms();
    size_t rc;
    while(1){
        rc = readPacket(buffer, sizeof(buffer), &address);
        if (rc >0){
            COAPPacket* p = COAPPacket::parse(buffer, rc, address.c_str());
            if (p != 0){
                oic_server->handleMessage(p);
                delete p;
            }
        }
        oic_server->sendQueuedPackets();
        if ((get_current_ms() - lastTick) > 50){
            lastTick = get_current_ms();
            oic_server->checkPackets();
        }
    }
}


void send_packet_addr(sockaddr_in destination, COAPPacket* packet){

    uint8_t buffer[1024];
    size_t response_len;
    socklen_t l = sizeof(destination);
    packet->build(buffer, &response_len);
    sendto(m_socketFd, buffer, response_len, 0, (struct sockaddr*)&destination, l);
}

void send_packet(COAPPacket* packet){
    String destination = packet->getAddress();
    size_t pos = destination.find(" ");
    String ip = destination.substr(0, pos);
    uint16_t port = atoi(destination.substr(pos).c_str());

    struct sockaddr_in client;

    client.sin_family = AF_INET;
    client.sin_port = htons(port);
    client.sin_addr.s_addr = inet_addr(ip.c_str());

    send_packet_addr(client, packet);
}

int main(int argc, char* argv[])
{
    cout << "Welcome in IoT CLI" << endl;
    oic_server = new OICClient([&](COAPPacket* packet){
        send_packet(packet);
    });
    oic_server->start("","");
    pthread_create(&m_thread, NULL, run, 0);


    while(1){
        cout << "> ";
        string line;
        cin >> line;

        if (line == "q" || line == "quit"){
            return 0;
        }
        if (line == "h" || line == "help"){
            showHelp();
        }
        if (line == "s" || line == "scan"){
            scan();
        }




    }

    return 0;
}

