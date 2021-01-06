#include <Wire.h>        // Library  I2C
#include <DFRobot_LCD.h> // Library LCD
#include <DHT.h>         // Library Hum&Temp

#define DS3231_I2C_ADDRESS 0x68 // RTC Address
#define DHTPIN 7                // Hum & Temp Pin (DHT Sensor)
#define DHTTYPE DHT11           // DHT 11  sensor

DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor for normal 16mhz Arduino
DFRobot_LCD lcd(16, 2);   //LCD 16 characters and 2 lines of show

//Variables
int chk;
float hum, temp;           //Stores humidity and temp value
int trigPin = 11;          // Trigger Pin  (Ultrasonic Sensor)
int echoPin = 12;          // Echo    Pin  (Ultrasonic Sensor)
long duration, cm, inches; //Stores distance values

// RTC Converters

byte decToBcd(byte val) // Convert normal decimal numbers to binary coded decimal
{
    return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) // Convert binary coded decimal to normal decimal numbers
{
    return ((val / 16 * 10) + (val % 16));
}

void setup()
{
    Wire.begin();
    lcd.init(); //  lcd.begin(16,2);
    dht.begin();
    Serial.begin(9600);
    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
}

//   set the initial time here:
//   DS3231 seconds, minutes, hours, day, date, month, year
//  setDS3231time(30,20,1,6,19,6,20);

void setDS3231time(byte second, byte minute, byte hour, byte dayOfWeek, byte dayOfMonth, byte month, byte year)
{
    // sets time and date data to DS3231
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0);                    // set next input to start at the seconds register
    Wire.write(decToBcd(second));     // set seconds
    Wire.write(decToBcd(minute));     // set minutes
    Wire.write(decToBcd(hour));       // set hours
    Wire.write(decToBcd(dayOfWeek));  // set day of week (1=Sunday, 7=Saturday)
    Wire.write(decToBcd(dayOfMonth)); // set date (1 to 31)
    Wire.write(decToBcd(month));      // set month
    Wire.write(decToBcd(year));       // set year (0 to 99)
    Wire.endTransmission();
}

void displayDistance()
{
    // The sensor is triggered by a HIGH pulse of 10 or more microseconds.
    // Give a short LOW pulse beforehand to ensure a clean HIGH pulse:
    digitalWrite(trigPin, LOW);
    delayMicroseconds(5);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);

    // Read the signal from the sensor: a HIGH pulse whose
    // duration is the time (in microseconds) from the sending
    // of the ping to the reception of its echo off of an object.
    pinMode(echoPin, INPUT);
    duration = pulseIn(echoPin, HIGH);

    // Convert the time into a distance
    cm = (duration / 2) / 29.1;   // Divide by 29.1 or multiply by 0.0343
    inches = (duration / 2) / 74; // Divide by 74 or multiply by 0.0135

    Serial.print("Distance: ");
    Serial.print(inches);
    Serial.print("in, ");
    Serial.print(cm);
    Serial.print("cm");
    Serial.println();
    delay(250);
}

void readDS3231time(byte *second,
                    byte *minute,
                    byte *hour,
                    byte *dayOfWeek,
                    byte *dayOfMonth,
                    byte *month,
                    byte *year)
{
    Wire.beginTransmission(DS3231_I2C_ADDRESS);
    Wire.write(0); // set DS3231 register pointer to 00h
    Wire.endTransmission();
    Wire.requestFrom(DS3231_I2C_ADDRESS, 7);
    // request seven bytes of data from DS3231 starting from register 00h
    *second = bcdToDec(Wire.read() & 0x7f);
    *minute = bcdToDec(Wire.read());
    *hour = bcdToDec(Wire.read() & 0x3f);
    *dayOfWeek = bcdToDec(Wire.read());
    *dayOfMonth = bcdToDec(Wire.read());
    *month = bcdToDec(Wire.read());
    *year = bcdToDec(Wire.read());
}

void displayTime()
{
    byte second, minute, hour, dayOfWeek, dayOfMonth, month, year;
    // retrieve data from DS3231
    readDS3231time(&second, &minute, &hour, &dayOfWeek, &dayOfMonth, &month,
                   &year);
    // send it to the serial monitor

    //  lcd.setCursor(6,1);
    Serial.print("Time: ");
    Serial.print(hour);
    Serial.print(":");
    if (minute < 10)
    {
        Serial.print("0");
    }
    Serial.print(minute);
    Serial.print(":");

    if (second < 10)
    {
        Serial.print("0");
    }
    Serial.println(second);

    //   Serial.setCursor(5,0);
    Serial.print("Date: ");
    Serial.print(month);
    Serial.print("/");
    Serial.print(dayOfMonth);
    Serial.print("/");
    Serial.println(year);
}

void displayTH()
{
    //Read data and store it to variables hum and temp
    hum = dht.readHumidity();
    temp = dht.readTemperature();
    //Print temp and humidity values to LCD

    float tempF = temp * 9.0 / 5.0 + 32.0;

    //    lcd.setCursor(2,2);
    Serial.print("Humidity: ");
    Serial.print(hum);
    Serial.println("%");
    //    Serial.setCursor(2,3);
    Serial.print("Temp: ");
    Serial.print(tempF);
    Serial.println("F");
}

void displayLCD()
{
    // when characters arrive over the serial port...
    if (Serial.available())
    {
        // wait a bit for the entire message to arrive
        delay(100);
        // clear the screen
        lcd.clear();
        // read all the available characters
        while (Serial.available() > 0)
        {
            // display each character to the LCD
            lcd.write(Serial.read());
        }
    }
}

void loop()
{

    displayTime(); // display the real-time clock data on the Serial Monitor,
    delay(1000);   // every second
    displayTH();
    delay(500);
    displayDistance();
    delay(500);
}