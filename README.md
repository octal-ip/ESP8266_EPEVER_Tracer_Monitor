# ESP8266_EPEVER_Tracer_Monitor
This project is an ESP8266 based monitoring device for EPEVER solar charge controllers. It queries the charge controller using MODBUS and then sends all statistics to InfluxDB/Grafana. The provided code is suited to the Platform.io development environment.

Four pieces of hardware are necessary:
- An ESP8266. I recommend using the Wemos D1 Mini, as they're small, cheap and well suited to this project.
- A logic level shifter to convert the 3.3v UART signals from the ESP8266 to 5V for the RS485 adaptor. 
- An RS485 adaptor. This project has been developed and tested with MAX486 based adaptors, many of which are available on eBay and Aliexpress.
- An old Ethernet cable you're willing to sacrifice. This will plug into the RJ45 jack on the Tracer and will also provide power from the Tracer to the other components.


#### Hardware wiring
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
