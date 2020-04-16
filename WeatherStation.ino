
/*  Revision 1.0
 * 
 *  Based on this site http://cactus.io/hookups/weather/anemometer/davis/hookup-arduino-to-davis-anemometer-software 
 * 
 *  Definition of windgust and average wind https://www.smhi.se/kunskapsbanken/meteorologi/hur-mats-vind-1.5924
 *     => Windgust - the highest 2 seconds momentary sample taken under one hour (this sketch reset the max, min values every ReportTimerShort)
 *     => Average Wind - The average wind during 10 minutes
 * 
 *  Main board: Wemos D1 mini - esp8266
 *  
 *  Sensors:  
 *  Temperature - DS18B20, 
 *  Anenometer and wind direction - Davis Anemometer 6410
 *  
 * 
 */

#include <PubSubClient.h> 
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define WindSensorPin D7  // The pin location of the Anemometer sensor
#define WindVanePin A0    // The pin location of the Wind vane sensor
#define One_Wire_Bus D5   // The pin location of the Thermometer sensor

#define VaneOffset 0;     // define the anemometer offset from magnetic north

//Constants
const char *wifi_ssid                    = "SSID";              // WiFi network SSID
const char *wifi_pwd                     = "PWD";               // WiFi network Password
const char *wifi_hostname                = "WeatherStation";
const char* mqtt_server                  = "IP";                // MQTT Boker IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port                      = 1883;                // MQTT Broker PORT, default is 1883 but can be anything.
const char *mqtt_user                    = "USER";              // MQTT Broker User Name
const char *mqtt_pwd                     = "PWD";               // MQTT Broker Password 
String clientId                          = "WeatherStation : " + String(ESP.getChipId(), HEX);
const char *ota_hostname                 = "WeatherStation";
const char *ota_pwd                      = "WeatherStation1234";

//Globals 
int VaneValue                            = 0;       // Raw analog value from wind vane
int Direction                            = 0;       // Translated 0 - 360 direction
int CalDirection                         = 0;       // Converted value with offset applied

bool debug                               = true;    // If true activate debug values to write to serial port

volatile unsigned long Rotations         = 0;       // Cup rotation counter used in interrupt routine
volatile unsigned long ContactBounceTime = 0;       // Timer to avoid contact bounce in isr
unsigned long NumOfWindSpeedSamples      = 0;       // Counts number of windspeed samples used in average wind calculation
unsigned long WindSampleTimePrevMillis   = 0;       // Store the previous millis   
unsigned long ReportTimerShortPrevMillis = 0;       // Store the previous millis
unsigned long ReportTimerLongPrevMillis  = 0;       // Store the previous millis
const unsigned long WindSampleTime       = 2000;    // Timer in milliseconds for windspeed (gust)calculation    
const unsigned long ReportTimerShort     = 60000;   // Timer in milliseconds (1 min) to report max/min wind    
const unsigned long ReportTimerLong      = 600000;  // Timer in milliseconds (10 min) to report temperature, average wind and restart max/min calculation      

float WindSpeed_mph                      = 0;       // Wind speed in miles per hour sampled during WindSampleTime
float WindSpeed_mps                      = 0;       // Wind speed in meter per second (m/s) sampled during WindSampleTime  
float MaxWindSpeed                       = 0;       // max windspeed during ReportIntervall
float MinWindSpeed                       = 100;     // min windspeed during ReportIntervall
float WindSpeedSummary                   = 0;       // Summarizing of all windspeed measurements to calculate average wind
float WindSpeedAverage                   = 0;       // Average wind measured during ReportIntervall
float Temperature                        = 0;       // Temperature sensor

// MQTT Constants
const char *WindSpeed_topic              = "WeatherStation/WindSpeed";
const char *WindSpeedAverage_topic       = "WeatherStation/WindSpeedAverage";
const char *MinWindSpeed_topic           = "WeatherStation/MinWindSpeed";
const char *MaxWindSpeed_topic           = "WeatherStation/MaxWindSpeed";
const char *WindVane_topic               = "WeatherStation/WindVane";
const char *WindVaneCompass_topic        = "WeatherStation/WindVaneCompass";
const char *Temperature_topic            = "WeatherStation/Temperature";

OneWire oneWire(One_Wire_Bus);                     // Setup a oneWire instance to communicate with any OneWire devices
DallasTemperature sensors(&oneWire);               // Pass the oneWire reference to Dallas Temperature sensor 

WiFiClient espClient;                              // Setup WiFi client definition WiFi
PubSubClient client(espClient);                    // Setup MQTT client



/**************************************************************************/
/* Setup                                                                  */
/**************************************************************************/

void setup() {
       
    if (debug) { Serial.begin(115200); Serial.println("WeatherStation"); }
    
    setup_wifi();
    setup_ota(); 
    sensors.begin();                    // Start up the Dallas Temperature library
    
    pinMode(WindSensorPin, INPUT_PULLUP);  
        
    attachInterrupt(digitalPinToInterrupt(WindSensorPin), rotation, FALLING); 
    
}



/**************************************************************************/
/* Setup WiFi connection                                                  */
/**************************************************************************/

void setup_wifi() {

    /*  WiFi status return values and meaning 
        WL_IDLE_STATUS      = 0,
        WL_NO_SSID_AVAIL    = 1,
        WL_SCAN_COMPLETED   = 2,
        WL_CONNECTED        = 3,
        WL_CONNECT_FAILED   = 4,
        WL_CONNECTION_LOST  = 5,
        WL_DISCONNECTED     = 6 */
  
    if (debug){ Serial.print("WiFi.status(): "); Serial.println(WiFi.status()); }
    
    int WiFi_retry_counter = 0;
   
    WiFi.hostname(wifi_hostname);
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifi_ssid, wifi_pwd);
    
    // Loop until reconnected or max retry then restart
    while (WiFi.status() != WL_CONNECTED){
        WiFi_retry_counter ++;
        if (WiFi_retry_counter == 30) {ESP.restart();}  
        if (debug){ Serial.print("WiFi retry: "); Serial.println(WiFi_retry_counter); } 
        delay(1000);
    }
    
    if (debug){ Serial.print("WiFi connected: ");Serial.println(WiFi.localIP()); }

}

/**************************************************************************/
/* Setup OTA connection                                                   */
/**************************************************************************/

void setup_ota() {

    ArduinoOTA.setHostname(ota_hostname); 
    ArduinoOTA.setPassword(ota_pwd);
    ArduinoOTA.onStart([]() {});
    ArduinoOTA.onEnd([]() {});
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
    ArduinoOTA.onError([](ota_error_t error) {
          // (void)error;
          // ESP.restart();
          });
    ArduinoOTA.begin();

}

/**************************************************************************/
/* Setup MQTT connection                                                   */
/**************************************************************************/

void reconnect() {

    int MQTT_retry_counter = 0;
    
    // Loop until reconnected or max retry then leave
    while (!client.connected() && MQTT_retry_counter < 30) {
       client.setServer(mqtt_server, mqtt_port);
       if (debug){ Serial.print("Connecting to MQTT server "); Serial.println(MQTT_retry_counter); }
       client.setServer(mqtt_server, mqtt_port);
       client.connect(clientId.c_str(), mqtt_user, mqtt_pwd);
       MQTT_retry_counter ++;
       delay (1000);
    }
    
    if (debug && client.connected()){ Serial.println(" MQTT connected"); }
    
}


void loop() {

    ArduinoOTA.handle();
    client.loop();
    if (WiFi.status() != WL_CONNECTED){ setup_wifi(); }             // Check WiFi connnection reconnect otherwise 
   
    if (!client.connected()) { reconnect(); }                       // Check MQTT connnection reconnect otherwise 
       
    if((millis() - WindSampleTimePrevMillis) > WindSampleTime ) {   // Calculate gust wind speed                  

        if (debug) { Serial.print("Rotations "); Serial.print(Rotations); }
        
        getWindSpeed();                                               
      
        if (debug) { 
            Serial.print("  WindSpeed (m/h)  "); Serial.print(WindSpeed_mph);
            Serial.print("  WindSpeed (m/s)  "); Serial.println(WindSpeed_mps);
        } 

        WindSampleTimePrevMillis = millis();        
    }
  
    if((millis() - ReportTimerShortPrevMillis) > ReportTimerShort ) {

       client.publish(MaxWindSpeed_topic, String(MaxWindSpeed).c_str());
       client.publish(MinWindSpeed_topic, String(MinWindSpeed).c_str());
        
        if (debug) { 
            Serial.print("Max WindSpeed     (m/s) "); Serial.println(MaxWindSpeed);
            Serial.print("Min WindSpeed     (m/s) "); Serial.println(MinWindSpeed);
        } 
        
        ReportTimerShortPrevMillis = millis();  
        resetMaxMinWindSpeed();                                      //Reset the max min values for the next report
   }


    if((millis() - ReportTimerLongPrevMillis) > ReportTimerLong ) {

        getWindDirection();                                           // INFO: Calling WindDirection to often causes WiFi disconnection due to that library also uses analog input
        getAverageWindSpeed(); 
        getTemperature();

        client.publish(Temperature_topic, String(Temperature).c_str());
        client.publish(WindSpeedAverage_topic, String(WindSpeedAverage).c_str());
        client.publish(WindVane_topic, String(CalDirection).c_str());
        client.publish(WindVaneCompass_topic, String(getHeading(CalDirection)).c_str());
       
        if (debug) { 
            Serial.print("Temperature             "); Serial.println(Temperature);
            Serial.print("Average WindSpeed (m/s) "); Serial.print(WindSpeedAverage);Serial.print("    Number of Samples  "); Serial.println(NumOfWindSpeedSamples);
            Serial.print("Winddirection           "); Serial.print(CalDirection); Serial.print(" "); Serial.print(getHeading(CalDirection)); Serial.print("\t"); Serial.println(getWindStrength(WindSpeed_mph));   
            Serial.println("");
        } 
        
        ReportTimerLongPrevMillis = millis();
        resetAverageWindSpeed();
   }

}

ICACHE_RAM_ATTR void rotation() {
    
      if((millis() - ContactBounceTime) > 15 ) {                      // Debounce the switch contact.
        Rotations++;
        ContactBounceTime = millis();
      }
}


void getWindSpeed() {
                      
      // convert to mp/h using the formula V=P(2.25/T)
      // V = P(2.25/2.0) = P * 1.125
      WindSpeed_mph = Rotations * (2.25/(WindSampleTime/1000));        // Windspeed in miles per hour (MPH)
      WindSpeed_mps = WindSpeed_mph * 0.44704;                         // Windspeed in meter per second (m/s) 
      
      Rotations = 0;   
      
      if (WindSpeed_mps < 50) { // && WindSpeed_mps > 0) {             // Filter out unnormal triggered rotations by filter out all results <50m/s AND >0m/s
            WindSpeedSummary =  WindSpeedSummary + WindSpeed_mps;      // Summarizing all windspeed samples for average calculation
            NumOfWindSpeedSamples++;                                   // Increse number of samples for average calculation
        
            if ( WindSpeed_mps > MaxWindSpeed) {                       // Store largest windspeed sample     
                MaxWindSpeed = WindSpeed_mps;   
                }
            
            if ( WindSpeed_mps < MinWindSpeed) {                       // Store smallest windspeed sample     
                MinWindSpeed = WindSpeed_mps;   
                }
      }  
}

void getAverageWindSpeed() {
    
     if (NumOfWindSpeedSamples > 0) { WindSpeedAverage = WindSpeedSummary / NumOfWindSpeedSamples; }  //  Calculate average windspeed, if rotations is 0 the division fails 
        else {WindSpeedAverage = 0;}                                                                           
}

void resetAverageWindSpeed() {

    WindSpeedAverage = 0;
    NumOfWindSpeedSamples = 0;
    WindSpeedSummary = 0;
}


void resetMaxMinWindSpeed() {
    
    MaxWindSpeed = 0;
    MinWindSpeed = 100;
}

void getTemperature(){

    sensors.requestTemperatures();                // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
    Temperature = sensors.getTempCByIndex(0);
}


void getWindDirection() {

    VaneValue = analogRead(WindVanePin);
    Direction = map(VaneValue, 0, 1023, 0, 359);
    CalDirection = Direction + VaneOffset;
    
    if(CalDirection > 360)
        CalDirection = CalDirection - 360;
    
    if(CalDirection < 0)
        CalDirection = CalDirection + 360;
}


String getHeading(int direction) {
   
    String CompassHeading; 
    
    if(direction < 22)          CompassHeading = "N";
    else if (direction < 67)    CompassHeading = "NE";
    else if (direction < 112)   CompassHeading = "E";
    else if (direction < 157)   CompassHeading = "SE";
    else if (direction < 212)   CompassHeading = "S";
    else if (direction < 247)   CompassHeading = "SW";
    else if (direction < 292)   CompassHeading = "W";
    else if (direction < 337)   CompassHeading = "NW";
    else                        CompassHeading = "N";
    
    return CompassHeading;
}

// converts windspeed miles per hour (MPH) to wind strength
String getWindStrength(float speed) {

    String WindStrength;
        
    if(speed < 2)                       WindStrength = "Calm";
    else if(speed >= 2 && speed < 4)    WindStrength = "Light Air";
    else if(speed >= 4 && speed < 8)    WindStrength = "Light Breeze";
    else if(speed >= 8 && speed < 13)   WindStrength = "Gentle Breeze";
    else if(speed >= 13 && speed < 18)  WindStrength = "Moderate Breeze";
    else if(speed >= 18 && speed < 25)  WindStrength = "Fresh Breeze";
    else if(speed >= 25 && speed < 31)  WindStrength = "Strong Breeze";
    else if(speed >= 31 && speed < 39)  WindStrength = "Near Gale";
    else                                WindStrength = "RUN...";

    return WindStrength;
}
