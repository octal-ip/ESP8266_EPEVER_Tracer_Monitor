#include <Arduino.h>
#include <ModbusMaster.h>
#include <ArduinoOTA.h>
#include <RunningAverage.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <InfluxDbClient.h>
#include <TelnetStream.h>

#include <secrets.h> /*Edit this file to include the following details.

#define SECRET_SSID "ssid"
#define SECRET_PASS "password"
#define SECRET_INFLUXDB_URL "influxdb-url"
#define SECRET_INFLUXDB_TOKEN "toked-id"
#define SECRET_INFLUXDB_ORG "org"
#define SECRET_INFLUXDB_BUCKET "bucket"
*/

InfluxDBClient client(SECRET_INFLUXDB_URL, SECRET_INFLUXDB_ORG, SECRET_INFLUXDB_BUCKET, SECRET_INFLUXDB_TOKEN);

#define MAX485_DE 12
#define MAX485_RE 14

/*
"Real-time data" and "Real-time status" registers
    PV array voltage             3100
    PV array current             3101
    PV array power               3102-3103
    Battery voltage              3104 
    Battery charging current     3105
    Battery charging power       3106-3107
    Load voltage                 310C
    Load current                 310D
    Load power                   310E-310F        
    Battery temperature          3110
    Internal temperature         3111
    Heat sink temperature        3112   
    Battery SOC                  311A
    Remote battery temperature   311B
    System rated voltage         311C
    Battery status               3200
    Charging equipment status    3201
    Discharging equipment status 3202
*/

byte avSamples = 10;
int failures = 0; //The number of failed operations. Will automatically restart the ESP if too many failures occurr in a row.

struct stats{
   const char *name;
   byte address;
   int type; //Whether the result is 16 or 32 bit number.
   float value;
   RunningAverage average;
   float multiplier;
   Point measurement;
};

stats arrstats[14] = {
  //Register name, MODBUS address, integer type (0 = uint16_t​, 1 = uint32_t​), value, running average, multiplier, InfluxDB measurement)
  {"PV_array_voltage", 0x00, 0, 0.0, RunningAverage(avSamples), 0.01, Point("PV_array_voltage")},
  {"PV_array_current", 0x01, 0, 0.0, RunningAverage(avSamples), 0.01, Point("PV_array_current")},
  {"PV_array_power", 0x02, 1, 0.0, RunningAverage(avSamples), 0.01, Point("PV_array_power")},
  {"Battery_voltage", 0x04, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Battery_voltage")},
  {"Battery_charging_current", 0x05, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Battery_charging_current")},
  {"Battery_charging_power", 0x06, 1, 0.0, RunningAverage(avSamples), 0.01, Point("Battery_charging_power")},
  {"Load_voltage", 0x0C, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Load_voltage")},
  {"Load_current", 0x0D, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Load_current")},
  {"Load_power", 0x0E, 1, 0.0, RunningAverage(avSamples), 0.01, Point("Load_power")},
  {"Battery_temperature", 0x10, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Battery_temperature")},
  {"Internal_temperature", 0x11, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Internal_temperature")},
  {"Heatsink_temperature", 0x12, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Heatsink_temperature")},
  {"Battery_SOC", 0x1A, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Battery_SOC")},
  {"Remote_battery_temperature", 0x1A, 0, 0.0, RunningAverage(avSamples), 0.01, Point("Remote_battery_temperature")}
};

byte collectedSamples = 0;
unsigned long lastUpdate = 0;

/*
"Statistical parameter registers
    Max input voltage today      3300
    Min input voltage today      3301
    Max battery voltage today    3302
    Min battery voltage today    3303
    Consumed energy today        3304-3305
    Consumed energy this month   3306-3307
    Consumed energy this year    3308-3309
    Total consumed energy        330A-330B
    Generated energy today       330C-330D
    Generated energy this moth   330E-330F
    Generated energy this year   3310-3311
    Total generated energy       3312-3313
    Carbon dioxide reduction     3314-3315
    Battery voltage              331A
    Net battery current          331B-331C
    Battery temperature          331D
    Ambient temperature          331E
*/

// Create the ModbusMaster object
ModbusMaster EPEver;

void preTransmission() {
  digitalWrite(MAX485_RE, 1);
  digitalWrite(MAX485_DE, 1);
}

void postTransmission(){
  delayMicroseconds(200); //Wait for for the data to be sent before disabling the output driver.
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);
}

void setup()
{
  Serial.begin(115200);

  pinMode(MAX485_RE, OUTPUT);
  pinMode(MAX485_DE, OUTPUT);
    
  // Disable the MAX485 outputs.
  digitalWrite(MAX485_RE, 0);
  digitalWrite(MAX485_DE, 0);
  
  // ****Start ESP8266 OTA and Wifi Configuration****
  Serial.println();
  Serial.print("Connecting to "); Serial.println(SECRET_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  unsigned long connectTime = millis();
  Serial.print("Waiting for WiFi to connect");
  while (!WiFi.isConnected() && (unsigned long)(millis() - connectTime) < 5000) { //Wait for the wifi to connect for up to 5 seconds.
    delay(100);
    Serial.print(".");
  }
  if (!WiFi.isConnected()) {
    Serial.println();
    Serial.println("WiFi didn't connect, restarting...");
    ESP.restart(); //Restart if the WiFi still hasn't connected.
  }
  Serial.println();
  Serial.print("IP address: "); Serial.println(WiFi.localIP());

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  // Hostname defaults to esp8266-[MAC]
  ArduinoOTA.setHostname("esp8266-EPEver-monitor");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_SPIFFS
      type = "filesystem";
    }

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  // ****End ESP8266 OTA and Wifi Configuration****

  // EPEver Device ID 1
  EPEver.begin(1, Serial);

  // Callbacks to enable/disable the MAX485 IOs
  EPEver.preTransmission(preTransmission);
  EPEver.postTransmission(postTransmission);

  // Telnet log is accessible at port 23
  TelnetStream.begin();

  // Check the InfluxDB connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
    Serial.print("Restarting...");
    ESP.restart();
  }

}


void loop()
{
  ArduinoOTA.handle();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Attempting to reconnect... ");
    failures++;
    WiFi.begin(SECRET_SSID, SECRET_PASS);
    delay(1000);
  }

  if ((unsigned long)(millis() - lastUpdate) >= 30000) { //Every 30 seconds.
  
    TelnetStream.printf("\r\n\r\nWiFi signal strength is: %d\r\n", WiFi.RSSI());

    TelnetStream.println("30 seconds has passed. Reading the MODBUS...");
    uint8_t result;
    
    Serial.flush(); //Make sure the hardware serial buffer is empty before communicating over MODBUS.
    
    EPEver.clearResponseBuffer();
    result = EPEver.readInputRegisters(0x3100, 18); // Read registers starting at 0x3100 for the realtime data.
    
    if (result == EPEver.ku8MBSuccess) {
      TelnetStream.println("MODBUS read successful.");
      
      for (int i = 0; i < 14; i++) {  //Iterate through each of the MODBUS registers and obtain their values.
        ArduinoOTA.handle();
        if (arrstats[i].type == 0) { //If the register is a normal 16-bit integer...
          arrstats[i].value = (EPEver.getResponseBuffer(arrstats[i].address) * arrstats[i].multiplier); //Calculatge the actual value.
        }
        else if (arrstats[i].type == 1) { //If the register is a 32-bit integer...
          arrstats[i].value = (EPEver.getResponseBuffer(arrstats[i].address) | (EPEver.getResponseBuffer(arrstats[i].address+1) << 8)) * arrstats[i].multiplier;  //Calculatge the actual value.
        }
        
        arrstats[i].average.addValue(arrstats[i].value); //Add the value to the running average.

        TelnetStream.printf("%s: %.1f, Average: %.1f, Samples: %d \r\n", arrstats[i].name, arrstats[i].value, arrstats[i].average.getAverage(), arrstats[i].average.getCount());

        if (arrstats[i].average.getCount() >= avSamples) { //Once we've collected enough samples, send the data to InfluxDB and flush the averages.
          arrstats[i].measurement.clearFields();
          arrstats[i].measurement.clearTags();
          arrstats[i].measurement.addTag("sensor", "EPEver-solar");
          arrstats[i].measurement.addField("value", arrstats[i].average.getAverage());

          TelnetStream.print("Sending data to InfluxDB: ");
          TelnetStream.println(client.pointToLineProtocol(arrstats[i].measurement));
          
          if (!client.writePoint(arrstats[i].measurement)) {
            failures++;
            TelnetStream.print("InfluxDB write failed: ");
            TelnetStream.println(client.getLastErrorMessage());
          }
          else if (failures >= 1) failures --;
          
          arrstats[i].average.clear();
        }
        yield();
      }
    }
    else {
      TelnetStream.print("MODBUS read failed. Returned value: "); TelnetStream.println(result);
      failures++;
      TelnetStream.print("Failure counter: "); TelnetStream.println(failures);
    }

    lastUpdate = millis();
  }

  if (failures >= 20) {
    Serial.print("Too many failures, rebooting...");
    TelnetStream.print("Failure counter has reached: "); TelnetStream.print(failures); TelnetStream.println(". Rebooting...");
    ESP.restart(); //Reboot the ESP if too many problems have been counted.
  }
}
