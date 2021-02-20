#include <Arduino.h>
#include <ArduinoJson.h> // arduino-cli lib install ArduinoJson
#include <DHT.h>
#include "ed_node.h"

#define DHT_PIN 4
#define DHTTYPE DHT11
#define SENSOR_READ_TIMEOUT 5000 //ms

DHT dht(DHT_PIN, DHTTYPE);



void after_init(){
    dht.begin();
}


/**
 * 
 * Write reason for failed deserialization and print to serial.
 * **/
void handle_deserialize_error(DeserializationError *err)
{
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(err->f_str());
}

void parse_data(const JsonDocument& payload){

}


/**
 * 
 * Use DHT11 to calculate temp, humidity, and heat index
 * 
 * Try reading from sensor, with 5s timeout.
 * 
 * **/
void generate_response(JsonDocument& response, JsonArray& data)
{

    float humidity, temperature, heat_index;

    response["time"] = 123456;


    // continue reading until values populate sensor
    // timeout after SENSOR_READ_TIMEOUT
    unsigned long now = millis();
    do{
        humidity = dht.readHumidity();
        temperature = dht.readTemperature();
        if((millis()-now) >= SENSOR_READ_TIMEOUT){
            response["status"] = StatusCodes::SensorReadTimeout;
            return;
        }
        delay(1);
    } while (isnan(humidity) || isnan(temperature));
    

    heat_index = dht.computeHeatIndex(temperature,humidity,false);
    data.add(temperature);
    data.add(humidity);
    data.add(heat_index);
}

