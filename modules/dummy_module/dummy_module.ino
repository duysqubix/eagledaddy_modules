#include "globals.h"
#include <Arduino.h>
#include <string.h>

uint8_t toggle = 0;

RecieveFrame g_RxFrame;
char str[100];

byte decToBcd(byte val) // Convert normal decimal numbers to binary coded decimal
{
    return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) // Convert binary coded decimal to normal decimal numbers
{
    return ((val / 16 * 10) + (val % 16));
}

void reverse(uint8_t arr[], uint8_t n)
{
    for (uint8_t low = 0, high = n - 1; low < high; low++, high--)
    {
        uint8_t temp = arr[low];
        arr[low] = arr[high];
        arr[high] = temp;
    }
}


void parse_rx_packet()
{
    uint8_t size = sizeof(uint64_t);
    uint8_t addr[size], data[size];

    for (int i = 0; i < size; i++)
    {
        addr[i] = RX_PACKET[i + 1]; // store address values of packet
    }

    for (int i = 0; i < MAX_RF_DATA_LEN; i++)
    {
        data[i] = RX_PACKET[i + 12]; // store actual rf data of packet
    }

    reverse(addr, size);                        // reverse as AVR stores integers little-endian, and incoming data is big-endian
    memcpy(&g_RxFrame.source_addr, addr, size); // store source addr to rx frame

    g_RxFrame.recv_opts = RX_PACKET[11]; // store recieve options

    memcpy(&g_RxFrame.rf_data, data, MAX_RF_DATA_LEN); // copy rf data to
}

void transmit_request(uint8_t *data, uint8_t len)
{
    uint8_t packet_len = len + 18;
    uint8_t packet[packet_len];

    char str[100];
    sprintf(str, "addr: %d, opts: %d, rf_data: %d", g_RxFrame.source_addr, g_RxFrame.recv_opts, g_RxFrame.rf_data);
    Serial.println(str);

    packet[0] = 0x7e;
    packet[1] = ((packet_len - 4) >> 8) & 0xff;
    packet[2] = (packet_len - 4) & 0xff;
    packet[3] = 0x10;
    packet[4] = 0x01; // frame_id =0, no respnose frame sent to this device
    packet[5] = (g_RxFrame.source_addr >> 56) & 0xff;
    packet[6] = (g_RxFrame.source_addr >> 48) & 0xff;
    packet[7] = (g_RxFrame.source_addr >> 40) & 0xff;
    packet[8] = (g_RxFrame.source_addr >> 32) & 0xff;
    packet[9] = (g_RxFrame.source_addr >> 24) & 0xff;
    packet[10] = (g_RxFrame.source_addr >> 16) & 0xff;
    packet[11] = (g_RxFrame.source_addr >> 8) & 0xff;
    packet[12] = g_RxFrame.source_addr & 0xff;
    packet[13] = 0xff;
    packet[14] = 0xfe;
    packet[15] = 0; // broadcast radius
    packet[16] = 0; // transmit options

    for (uint8_t i = 0; i < len; i++)
    {
        packet[17 + i] = *data;
        data++;
    }

    uint8_t chksum = 0;
    for (uint8_t i = 3; i < packet_len - 1; i++)
    {
        chksum = chksum + packet[i];
    }

    packet[packet_len - 1] = 0xff - chksum;

    uint8_t tries = 0;
    const uint8_t max_tries = 3;
    // now attempt to send it, it will try a max of three times
    // if it doesn't recieve a transmit status report, within the alloted timeout and max tries has been attempted,
    // it will silently discard everything, and return to normal mode

    uint8_t status[11];
    bool success = false;
    while (tries < max_tries)
    {
        for (uint8_t i = 0; i < sizeof(packet); i++)
        {
            Serial.write(packet[i]);
        }

        Serial.flush();

        if (!Serial.readBytes(status, sizeof(status)))
        {
            // attempt gone wrong, retry
            tries++;
            continue;
        }
        else
        {
            // success full we can do some extra checks here
            if (status[3] == 0x8b)
                break; // looks like a successful transmit
            tries++;
            continue;
        }
    }
}

enum Commands
{
    RequestPing = 0x1d,
    RequestToggle = 0x2b,
    RequestInt = 0x3c,
    RequestFloat = 0x4a,
};

void process_cmd(MasterRequest *request)
{
    uint8_t cmd = request->cmd;
    // Send back ping/i'm here command
    if (cmd == RequestPing)
    {
        uint8_t to_send[] = {cmd, 0x06};
        transmit_request(to_send, sizeof(to_send));
    }

    else if (cmd == RequestToggle){
        toggle ^= _BV(0);
        uint8_t to_send[] = {cmd, toggle};
        transmit_request(to_send, sizeof(to_send));
    }

    else if (cmd == RequestInt){
        uint8_t num = random(0x00, 0xff);
        uint8_t to_send[] = {cmd, num};
        transmit_request(to_send, sizeof(to_send));
    }

    else if (cmd == RequestFloat){
        int rand = random(0,500);
        float num = (float)rand / 100.0;
        uint8_t to_send[sizeof(float)+1];
        to_send[0] = cmd;
        memcpy(to_send+1, &num, sizeof(float));
        transmit_request(to_send, sizeof(to_send));
    }
     
    else
    {
        uint8_t to_send[3] = {0xff, 0x2}; // unknown command
        to_send[2] = cmd;
        transmit_request(to_send, sizeof(to_send));
    }
}

void handle_packets()
{
    switch (RX_PACKET[0])
    {
    case 0x90:
        MasterRequest request;
        parse_rx_packet(); // populates g_RxFrame

        // parse out the request from master here
        memcpy(&request, &g_RxFrame.rf_data, MAX_RF_DATA_LEN);    
        process_cmd(&request);

        break;
    default:
        break;
    }
}

void setup()
{

    randomSeed(analogRead(0));

    Serial.begin(115200);
    Serial.setTimeout(500);
    while (!Serial)
        ;

}

void loop()
{

    if (Serial.available() > 0)
    {
        for (;;)
        {
            RX_BUF = Serial.read();
            if (RX_BUF == 0x7e)
                break;
        }
        Serial.readBytes(HEADER, 2);

        uint16_t buf_len = (HEADER[0] << 8) | HEADER[1];

        Serial.readBytes(RX_PACKET, buf_len + 1);

        handle_packets();
    }
}
