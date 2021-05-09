# WeatherStation
An [Wemos D1 Mini](https://www.wemos.cc/en/latest/) based wind and temperature station reporting direction, max, min and average wind via MQTT topics

## Installation
Needed Arduino Libraries to be included in [IDE](https://www.arduino.cc/en/Main/Software). Install them either from GitHub repository directly or within the IDE application itself **Sketch > Import Library** 

| Library                            | Link to GitHub                                      |
| ---------------------------------- | --------------------------------------------------- |
| PubSubClient                       |  https://github.com/knolleary/pubsubclient          |      
| OneWire                            |                                                     |
| Dallas Temperature                 |                                                     |                                                     

## MQTT Topics
MQTT Topics to be published. 

| Topic                              | Description                                         |
| ---------------------------------- | --------------------------------------------------- |
| WeatherStation/WindSpeed           |  set topic - Gust wind speed (m/s) during `ReportTimerShort`      |
| WeatherStation/WindSpeedAverage    |  set topic - Average wind speed (m/s) during `ReportTimerLong`   |
| WeatherStation/MinWindSpeed        |  set topic - Min gust wind speed (m/s) during `ReportTimerShort`  |
| WeatherStation/MaxWindSpeed        |  set topic - Max gust wind speed (m/s) during `ReportTimerShort`  |
| WeatherStation/WindVane            |  set topic - Wind direction in degrees 0-360                   |
| WeatherStation/WindVaneCompass     |  set topic - Wind compass direction e.g. N, NE                 |
| WeatherStation/Temperature         |  set topic - Temperature in degrees                             |

## Wiring
<img src="https://github.com/MagnusPer/WeatherStation/blob/master/images/WeatherStation.jpg" width="800">



## BOM List
| Part                               | Comment/Link                                        |
| ---------------------------------- | --------------------------------------------------- |
|  Wemos D1 mini                     | https://www.wemos.cc/en/latest/                     |   
|  Anemometer Davis 6410             | https://www.davisinstruments.com/product/anemometer-for-vantage-pro2-vantage-pro/ |
|  Tempsensor DB18B20                |                                                     |  
|  Power Supply HLK-PM03             | http://www.hlktech.net/product_detail.php?ProId=59  |  


## Features
 - Max, min, average wind speed
 - Wind direction
 - Temperature 
 
## References
- http://cactus.io/hookups/weather/anemometer/davis/hookup-arduino-to-davis-anemometer-software 
