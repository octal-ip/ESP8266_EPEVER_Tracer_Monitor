# ESP8266_EPEVER_Tracer_Monitor
This project is an ESP8266 based monitoring device for EPEVER solar charge controllers. It queries the MPPT charge controller using MODBUS and then sends all statistics to InfluxDB/Grafana. Provided code is suited to the Platform.io development environment.

Four pieces of hardware are necessary:
- An ESP8266. I recommend using the Wemos D1 Mini, as they're small, cheap and well suited to this project.
- A logic level shifter to convert the 3.3v UART signals from the ESP8266 to 5V for the RS485 adaptor.
- An RS485 adaptor. This project has been developed and tested with MAX486 based adaptors, many of which are available on eBay and Aliexpress.
- An old Ethernet cable you're willing to sacrifice. This will plug into the RJ45 jack on the Tracer and will also provide power from the Tracer to the other components.

Hardware is connected together as follows:
3.3v pin on Wemos D1 Mini -> LV pin on logic level shifter.
5v pin on Wemons D1 Mini -> HV pin on logic level shifter.
D6 pin on Wemos D1 Mini -> LV1 pin on logic level shifter.
D5 pin on Wemos D1 Mini -> LV2 pin on logic level shifter.
Ground pin on Wemos D1 Mini -> Ground pin on logic level shifter.
TX pin on Wemos D1 Mini -> LV3 pin on logic level shifter.
RX pin on Wemos D1 Mini -> LV4 pin on logic level shifter.

HV1 pin on logic level shifter -> DE pin on MAX485 board.
HV2 pin on logic level shifter -> RE pin on MAX485 board.
HV3 pin on logic level shifter -> DI pin on MAX485 board.
HV4 pin on logic level shifter -> RO pin on MAX485 board.

Pin 2 of the RJ45 connector -> VCC pin on MAX486 board, HV pin on on logic level shifter, 5v pin on Wemons D1 Mini.
Pin 8 of the RJ45 connector -> GND pin on MAX486 board, GND pin on on logic level shifter, GND pin on Wemons D1 Mini.
Pin 6 of the RJ45 connector -> A pin on MAX486 board.
Pin 4 of the RJ45 connector -> B pin on MAX486 board.
