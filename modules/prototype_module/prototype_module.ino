#include "globals.h"
#include <Arduino.h>
#include <DHT.h> // Library Hum&Temp
#include <EEPROM.h>
#include <Wire.h>   // Library  I2C
#include <ds3231.h> // RTC
#include <string.h>
//#include <SoftwareSerial.h>
//#include <SerLCD.h>

#define DS3231_I2C_ADDRESS 0x68 // RTC Address
#define DHTPIN 7                // Hum & Temp Pin (DHT Sensor)
#define DHTTYPE DHT11           // DHT 11  sensor
#define TRIG_PIN 9
#define ECHO_PIN 10
#define RELAY_PIN 11

RecieveFrame g_RxFrame;
DHT dht(DHTPIN, DHTTYPE);
struct ts rtc;
ScheduleTime times[MAX_SCHEDULE_TIMES];
unsigned long current_millis, start_millis, motor_start, motor_time, motor_duration, cooldown_start;
bool motor_on = false;
bool cooldown = false;
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

void refresh_schedule()
{

    // read from eeprom, and fill Times array
    uint8_t eeprom_addr = EEMEM_MOTOR_SCH_ADDR;
    uint8_t values[MAX_SCHEDULE_TIMES * 2];
    uint8_t a, b;

    for (uint8_t i = 0; i < MAX_SCHEDULE_TIMES * 2; i++)
    {
        values[i] = EEPROM.read(eeprom_addr++);
    }

    for (uint8_t i = 0; i < MAX_SCHEDULE_TIMES; i++)
    {
        a = (i * 2);
        b = (1 * 2) + 1;
        ScheduleTime t;
        if (values[a] != 0xff)
        {
            t = {
                .hour = *(values + a),
                .min = *(values + a + 1)};
        }
        else
        {
            t = {
                .hour = 0xff,
                .min = 0xff};
        }

        times[i] = t;
    }

    // to test this now...
    //    for (int i = 0; i < MAX_SCHEDULE_TIMES; i++) {
    //        ScheduleTime t = times[i];
    //        sprintf(str, "%d:%d MOTOR_TIME: %d", t.hour, t.min, EEPROM.read(EEMEM_MOTOR_TIME_ADDR));
    //        Serial.println(str);
    //    }
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
    RequestTime = 0x1d,
    RequestTH = 0x2b,
    RequestDist = 0x3c,
    RequestMotor = 0x4a,
    SetTime = 0x5e,
    SetSchedule = 0x6c,
    SetMotorTime = 0x7a,
};

void process_cmd(MasterRequest *request)
{
    uint8_t cmd = request->cmd;

    // Send back temperature and Humditity
    //
    // Response packet in the form of:
    // [mod_id, temp, hum]; where temp in C, hum = % both uint16_t
    if (cmd == RequestTH)
    {
        //float temp = dht.readTemperature();
        //float hum = dht.readHumidity();

        float temp, hum;

        while (1)
        {
            temp = dht.readTemperature();
            hum = dht.readHumidity();

            if (isnan(temp) || isnan(hum))
            {
                continue;
            }
            break;
        }

        uint8_t *t, *h;
        t = (uint8_t *)(&temp);
        h = (uint8_t *)(&hum);

        uint8_t to_send[sizeof(float) * 2];

        memcpy(to_send, t, sizeof(float));
        memcpy(to_send + sizeof(float), h, sizeof(float));

        transmit_request(to_send, sizeof(to_send));
    }

    // Set the amount of time motor is on
    else if (cmd == SetMotorTime)
    {
        uint8_t mt = request->args[0];
        EEPROM.update(EEMEM_MOTOR_TIME_ADDR, mt);

        uint8_t to_send[] = {0x00, 0x1a, 'O', 'K', '\r', '\n'};
        transmit_request(to_send, sizeof(to_send));
    }

    // Overwrite schedule times for when to switch 'motor' on
    //
    // data expected in the following format:
    //
    // [uin16_t*4]// if 0xffff, then ignore
    else if (cmd == SetSchedule)
    {
        uint8_t eeprom_addr = EEMEM_MOTOR_SCH_ADDR;
        uint8_t times[MAX_SCHEDULE_TIMES * 2];

        memcpy(times, request->args, MAX_SCHEDULE_TIMES * 2);

        for (uint8_t i = 0; i < MAX_SCHEDULE_TIMES * 2; i++)
        {
            if (request->args[i] != 0xff)
            {

                EEPROM.update(eeprom_addr, request->args[i]);
            }
            eeprom_addr++;
        }

        refresh_schedule();

        uint8_t to_send[] = {0x00, 0x1a, 'O', 'K', '\r', '\n'};
        transmit_request(to_send, sizeof(to_send));
    }

    // From given data set time on RTC module
    //
    // data expected in the following format
    //
    // [sec, min, hour, day, month, year]
    //   u8   u8   u8    u8   u8    u16
    // Respond with "OK"
    else if (cmd == SetTime)
    {

        // the data should be contained within rf_data;
        rtc.sec = request->args[0];
        rtc.min = request->args[1];
        rtc.hour = request->args[2];
        rtc.mday = request->args[3];
        rtc.mon = request->args[4];
        rtc.year = (uint16_t)((request->args[5] << 8) | request->args[6]);

        DS3231_set(rtc);

        uint8_t to_send[] = {0x00, 0x1a, 'O', 'K', '\r', '\n'};
        transmit_request(to_send, sizeof(to_send));
    }

    // Send back real time as described by World Clock
    //
    // Response:
    // [mod_id, u64 value that contains time]
    else if (cmd == RequestTime)
    {
        uint8_t time[9];

        //    Wire.beginTransmission(DS3231_I2C_ADDRESS);
        //    Wire.write(0);
        //    Wire.endTransmission();
        //    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);

        //    time[0] = bcdToDec(Wire.read() & 0x7f); // second
        //    time[1] = bcdToDec(Wire.read()); // minute
        //    time[2] = bcdToDec(Wire.read() & 0x3f); // hour
        //    time[3] = bcdToDec(Wire.read()); // day of week
        //    time[4] = bcdToDec(Wire.read()); // day of month
        //    time[5] = bcdToDec(Wire.read()); // month
        //    time[6] = bcdToDec(Wire.read()); // year

        DS3231_get(&rtc);
        time[0] = 0x00;
        time[1] = 0x1a;
        time[2] = rtc.sec;
        time[3] = rtc.min;
        time[4] = rtc.hour;
        time[5] = rtc.mday;
        time[6] = rtc.mon;
        time[7] = (uint8_t)((rtc.year >> 8) & 0xff);
        time[8] = (uint8_t)(rtc.year & 0xff);
        transmit_request(time, sizeof(time));
    }

    // Sends back running motor time when motor is running, uint16_t in secs
    //
    // Response:
    // [mod_id, time(s)]
    else if (cmd == RequestMotor)
    {
        digitalWrite(RELAY_PIN, !digitalRead(RELAY_PIN));

        uint8_t to_send[] = {0x00, 0x1a, '0', 'K', '\r', '\n'};
        transmit_request(to_send, sizeof(to_send));
    }

    // Sends back distance to feed from top of barrel as uint16_t in cm
    //
    //Repsonse:
    // [mod_id, distance(cm)]
    else if (cmd == RequestDist)
    {
        float cm, duration;
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(5);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);

        // Read the signal from the sensor: a HIGH pulse whose
        // duration is the time (in microseconds) from the sending
        // of the ping to the reception of its echo off of an object.
        duration = pulseIn(ECHO_PIN, HIGH);

        // Convert the time into a distance
        cm = (float)duration * 0.034 / 2.0; // Divide by 29.1 or multiply by 0.0343
        uint8_t to_send[sizeof(float) + 2];
        to_send[0] = 0x00;
        to_send[1] = 0x1a;
        memcpy(to_send + 2, &cm, sizeof(float));
        transmit_request(to_send, sizeof(to_send));
    }
    else
    {
        uint8_t to_send[5] = {0x00, 0x1a, 0xff, 0x2}; // unknown command
        to_send[4] = cmd;
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
        request.module_id = (g_RxFrame.rf_data[0] << 8) | g_RxFrame.rf_data[1];

        if (request.module_id != MOD_ID)
        {
            // first check to see if it isn't broadcast request
            // check for broadcast
            // Internal Broadcast command - sends back its module_id. Must use a random number
            // as this cmd is intended to be used when host uses 'broadcast' mode, we don't want
            // to saturate RF bus with multiple data bits
            if (request.module_id == BroadcastId)
            {
                uint8_t to_send[] = {0x00, 0x1a};
                delay(random(0, 4000)); // wait anywhere between 0ms and 2000ms
                transmit_request(to_send, sizeof(to_send));
                break;
            }

            // no? okay, then error out
            uint8_t err[] = {0x00, 0x1a, 0xff, 0x01}; // Error, Wrong Module ID
            transmit_request(err, 4);
            break;
        }

        process_cmd(&request);

        break;
    default:
        break;
    }
}

void setup()
{
    uint8_t motor_time;

    Wire.begin();
    dht.begin();
    DS3231_init(DS3231_INTCN);
    Serial.begin(115200);
    Serial.setTimeout(500);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    while (!Serial)
        ;
    refresh_schedule();

    start_millis = millis();
}

void loop()
{

    //unsigned long current = millis();
    //
    //if((unsigned long)(current-TIMER_START) >= INTERVAL){
    //    // Interval time has passed. Here you can query RTC and adjust
    //    // interval value to attempt to mimick passage of time accurately
    //    struct ts tmp;
    //    DS3231_get(&tmp);

    //    TIMER_START current;
    //}

    // if (motor_on) {
    //     motor_time = millis();
    //     if ((motor_time - motor_start) >= motor_duration) {
    //         motor_on = false;
    //         digitalWrite(RELAY_PIN, LOW);
    //         cooldown = true;
    //         cooldown_start = millis();
    //         Serial.println("MOTOR OFF");
    //     }
    // }

    // current_millis = millis();

    // if (cooldown) {
    //     if ((current_millis - cooldown_start) >= 60000) {
    //         cooldown = false;
    //     }
    // }

    // if ((current_millis - start_millis) >= 1000) {
    //     // refresh RTC and compare with schedule times - if there is a match,
    //     // turn on motor for N secs
    //     DS3231_get(&rtc);

    //     for (uint8_t i = 0; i < MAX_SCHEDULE_TIMES; i++) {
    //         ScheduleTime* t = &times[i];
    //         if (t->hour == rtc.hour && t->min == rtc.min) {
    //             // make sure we aren't in a cooldown period
    //             if (!cooldown && !motor_on) {
    //                 sprintf(str, "MOTOR ON (%d:%d)", rtc.hour, rtc.min);
    //                 // there is a match, turn it on and wait
    //                 Serial.println(str);
    //                 motor_on = true;
    //                 motor_start = millis();
    //                 digitalWrite(RELAY_PIN, HIGH);
    //                 motor_duration = EEPROM.read(EEMEM_MOTOR_TIME_ADDR) * 1000;
    //                 break;
    //             }
    //         }
    //     }

    //     start_millis = current_millis;
    // }

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
