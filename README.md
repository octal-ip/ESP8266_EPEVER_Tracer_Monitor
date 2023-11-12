# ESP8266_EPEVER_Tracer_Monitor
This project is an ESP8266 based monitoring device for EPEVER solar charge controllers. It queries the charge controller using MODBUS and then sends all statistics to InfluxDB/Grafana. The provided code is suited to the Platform.io development environment.

![Grafana Screenshot](https://github.com/octal-ip/ESP8266_EPEVER_Tracer_Monitor/blob/main/pics/Grafana_screenshot.png "Grafana Screenshot")


#### Hardware
- An ESP8266. I recommend using the Wemos D1 Mini, as they're small, cheap and well suited to this project.  
![Wemos D1 Mini](https://github.com/octal-ip/ESP8266_EPEVER_Tracer_Monitor/blob/main/pics/D1_mini.png "Wemos D1 Mini")
- A logic level shifter to convert the 3.3v UART signals from the ESP8266 to 5V for the RS485 adaptor.  
![Logic Level Shifter](https://github.com/octal-ip/ESP8266_EPEVER_Tracer_Monitor/blob/main/pics/logic_shifter.png "Logic Level Shifter")
- An RS485 adaptor. This project has been developed and tested with MAX486 based adaptors, many of which are available on eBay and Aliexpress.  
![MAX485 RS485 Adaptor](https://github.com/octal-ip/ESP8266_EPEVER_Tracer_Monitor/blob/main/pics/RS485_board.png "MAX485 RS485 Adaptor")
- An old Ethernet cable you're willing to sacrifice. This will plug into the RJ45 jack on the Tracer and will also provide power from the Tracer to the other components.


#### Wiring
| Wemos D1 Mini | Logic level shifter |
| ------------ | ------------ |
| D6 | LV1 |
| D5 | LV2 |
| TX  |  LV3 |
| RX  |  LV4 |

| Logic level shifter | MAX485 |
| ------------ | ------------ |
| HV1  | DE |
| HV2 |  RE |
| HV3 | DI |
| HV4 | RO |

| RJ45 connector | MAX485 | Logic level shifter | Wemos D1 Mini
| ------------ | ------------ | ------------ | ------------ |
| Pin 2 (T568B Orange) | VCC | HV | 5v |
| Pin 8 (T568B Brown) |  GND | GND | GND |
| Pin 6 (T568B Green) | A | - | - | 
| Pin 4 (T568B Blue) | B | - | - |


#### Credits:
[4-20ma for ModbusMaster](https://github.com/4-20ma/ModbusMaster)

[RobTillaart for RunningAverage](https://github.com/RobTillaart/RunningAverage)

[JAndrassy for TelnetStream](https://github.com/jandrassy/TelnetStream)

[Tobias Schürg for InfluxDB Client](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino/)


#### Release notes:
##### Nov 12, 2023:
	- Updated to use the official InfluxDB Client for Arduino, introducing support for InfluxDB v2
