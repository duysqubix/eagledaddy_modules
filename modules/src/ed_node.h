/**
 * 
 * Main EagleDaddy Node Library.
 * 
 * Designed in such a way to completely abstract the incoming messages
 * and provide three functions that must be defined for each individual `nodes` code.
 * Whether is be a soil moisture reader, temp/humidity sensor, or a toggle node for turning a device on/off.
 * 
 * The following hooks are by default undefined and must be unique to each node
 * 
 *  handle_deserialize_error
 *  parse_data
 *  generate_response
 * 
 * If left alone, this code will still compile, but will not necessary do anything useful. It is the responsbility
 * of programmer to generate a default 'report/response' when communicated too. The is only one commands known to ALL
 * nodes:
 * 
 *  0x69 <- which skips `generate_response` and sends back an empty packet (used for pings ...also.. nice)
 * 
 * Incoming Json Structure will be:
 * 
 * {
 *     "cmd": <cmd>,
 *     "value": "",
 * }
 * 
 * 
 * Response Json Structure will be:
 * 
 * {
 *     "cmd": <cmd>,
 *     "time": <time in ms>,
 *     "status": <error-code>,
 *     "data":[]
 * }
 * **/
#ifndef __GLOBALS_H__
#define __GLOBALS_H__


#define ABS_MAX_PAYLOAD_SIZE 256  // absolute maximum payload size for 900Mhz DigiMesh Device
#define RX_HEADER_SIZE 15           // header offset size before getting to rf_data
#define MAX_RF_DATA_LEN ABS_MAX_PAYLOAD_SIZE
#define MAX_RX_PACKET_LEN (MAX_RF_DATA_LEN+RX_HEADER_SIZE) // largest absolute size of incoming rcv  packet by an xbee
#define JSON_OBJ_SIZE 200

typedef unsigned long long uint64_t;
typedef unsigned long uint32_t;
typedef unsigned int uint16_t;
typedef unsigned char uint8_t;

enum StatusCodes{
    Ok=-1,         // ideal no error detected (default)
    SensorReadTimeout, // sensor failing to measure
};

enum Commands{
    Ping=0x69 // only command that skips generate_response hook and sends back empty packet
};


/**
 * 
 * Data structure for holding rcv rf_data.
 * **/
class RecieveFrame{
  public:
    uint64_t source_addr;
    uint8_t rf_data[MAX_RF_DATA_LEN];
    uint8_t rf_length;
    RecieveFrame(uint8_t *raw_pkt, uint8_t pkt_size);
};

void reverse(uint8_t arr[], uint8_t n); // reverse array order
void transmit_request(RecieveFrame *rcv, uint8_t *data, uint8_t len);   //tx request generator 
void process(RecieveFrame *frame);  // main processing logic 
void handle_packet(uint8_t *packet, uint8_t size);  // handle incoming packets, delegate based on FrameId
void loop();    // main arduino loop
void setup();   // main arduino setup
void parse_data(const JsonDocument &payload);   // hook for parsing incoming json object
void generate_response(JsonDocument& response, JsonArray& data); // hook for handling response json object
void handle_deserialize_error(DeserializationError *err); // hook for handling any errors from deserializing json
void after_init();  // extra init logic after `setup` has been called

// constructor
RecieveFrame::RecieveFrame(uint8_t *raw_pkt, uint8_t pkt_size){
      /**
       * Deconstructs raw array of chars into recieve frame
       * 
      packet layout
      1 byte: frame id (0x90)
      2-9 byte: src addr 
      10-11 byte: reserved
      12 bytes: rcv_opts 
      13-n: payload
      n+1: chksum
      **/
      uint8_t addr_size = sizeof(uint64_t);
      uint8_t _addr[addr_size];

      this->rf_length = pkt_size;
      for (size_t i=0; i<=addr_size;i++){
        *(_addr+i) = *(raw_pkt + i+1); 
      }

      reverse(_addr, addr_size); // reverse order so it can store address correctly as uint64_t
      memcpy(&this->source_addr, _addr, sizeof(uint64_t));


      memcpy(this->rf_data, raw_pkt+12, pkt_size); // store rf_data using pkt_size and offset needed to beging at rf_data

    }


/**
 * reverse order of an array
 * **/
void reverse(uint8_t arr[], uint8_t n)
{
    for (uint8_t low = 0, high = n - 1; low < high; low++, high--)
    {
        uint8_t temp = arr[low];
        arr[low] = arr[high];
        arr[high] = temp;
    }
}



/**
 * 
 * Main Transmit Request Generator
 * 
 * Constructs 0x10 (TX Request) via Digi Xbee API for 900Mhz.
 * 
 * Chksum is calculated by by adding all bytes (except delim, and length bytes)
 * subtracting result by 0xff and truncating to 8bit value
 * 
 * 
 * Simple three time polling method to send TX rquest, by monitoring status packet. 
 * **/
void transmit_request(RecieveFrame *rcv, uint8_t *data, uint8_t len)
{
    uint8_t packet_len = len + 18;
    uint8_t packet[packet_len];

    packet[0] = 0x7e;
    packet[1] = ((packet_len - 4) >> 8) & 0xff;
    packet[2] = (packet_len - 4) & 0xff;
    packet[3] = 0x10;
    packet[4] = 0x01; // frame_id =0, no respnose frame sent to this device
    packet[5] = (rcv->source_addr >> 56) & 0xff;
    packet[6] = (rcv->source_addr >> 48) & 0xff;
    packet[7] = (rcv->source_addr >> 40) & 0xff;
    packet[8] = (rcv->source_addr >> 32) & 0xff;
    packet[9] = (rcv->source_addr >> 24) & 0xff;
    packet[10] = (rcv->source_addr >> 16) & 0xff;
    packet[11] = (rcv->source_addr >> 8) & 0xff;
    packet[12] = rcv->source_addr & 0xff;
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



byte decToBcd(byte val) // Convert normal decimal numbers to binary coded decimal
{
    return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) // Convert binary coded decimal to normal decimal numbers
{
    return ((val / 16 * 10) + (val % 16));
}




/**
 * 
 * Allocate JsonDocuments for both RX and TX
 * 
 * Deserialize and handle any errors
 * 
 * call `handle_deserialize_error`
 * call `generate_response` hook
 * 
 * allocate buffer and store serialized json object
 * set serialized json as payload for transmit packet
 * **/
void process(RecieveFrame *frame)
{
    uint8_t *payload = frame->rf_data;

    StaticJsonDocument<JSON_OBJ_SIZE> rx_json;
    StaticJsonDocument<JSON_OBJ_SIZE> tx_json;
    DeserializationError error = deserializeJson(rx_json, payload);
    if (error)
    {
        handle_deserialize_error(&error);
        return;
    }

    tx_json["cmd"] = rx_json["cmd"];
    tx_json["status"] = StatusCodes::Ok;
    JsonArray data = tx_json.createNestedArray("data");

    if ((uint8_t)rx_json["cmd"] != Commands::Ping){
        generate_response(tx_json, data);
    }

    int json_size = (int)measureJson(tx_json);

    char response_payload[json_size+1]; // serialized json is now in this array
    serializeJson(tx_json, response_payload, sizeof(response_payload));

    // now send using transmit request
    transmit_request(frame, response_payload, sizeof(response_payload));
}


/**
 * 
 * Check FrameID to match against types of Frames.
 * The first position *packet is pointing to, will
 * indicate FrameID
 *  
 **/
void handle_packet(uint8_t *packet, uint8_t size){

    // RxFrame
    if(*packet == 0x90){
        RecieveFrame frame(packet, size);
        process(&frame);
    }

}


/**
 * 
 * Begin Serial, set to 9600 baud; 500ms timeout
 * 
 * **/
void setup()
{

    Serial.begin(9600);
    Serial.setTimeout(500);
    while (!Serial)
        ;

    after_init();
}


/**
 * 
 * Wait for incoming recieve packet,
 * parse out header (delim, len_01, len_02)
 * store rest of packet in buffer
 * **/
void loop()
{


    if (Serial.available() > 0)
    {
        uint8_t rcv_packet[MAX_RX_PACKET_LEN];
        uint8_t header[2];

        for (;;)
        {
            if (Serial.read() == 0x7e)
                break;
        }
        Serial.readBytes(header, 2);

        uint8_t packet_len = (uint8_t)((header[0] << 8) | header[1]);

        Serial.readBytes(rcv_packet, packet_len);
        handle_packet(rcv_packet, packet_len-12);
    }
}



#endif
