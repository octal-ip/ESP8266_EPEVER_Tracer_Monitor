//Choose which protocol you'd like to post the statistics to your database by uncommenting one (or more) of the definitions below.
#define INFLUX_UDP
//#define INFLUX_HTTP

#include <Arduino.h>
#include <ModbusMaster.h>
#include <ArduinoOTA.h>
#include <RunningAverage.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <TelnetPrint.h>

#include <secrets.h> /*Edit this file to include the following details.
SECRET_INFLUXDB only required if using HTTP mode.
SECRET_INFLUX_IP_OCTETx only required if using UDP mode.

#define SECRET_SSID "<ssid>>"
#define SECRET_PASS "<password>"
#define SECRET_INFLUXDB "http://<IP Address>:8086/write?db=<db name>&u=<username>&p=<password>"
#define SECRET_INFLUX_IP_OCTET1 <first IP octet>
#define SECRET_INFLUX_IP_OCTET2 <second IP octet>
#define SECRET_INFLUX_IP_OCTET3 <third IP octet>
#define SECRET_INFLUX_IP_OCTET4 <last IP octet>
*/

#ifdef INFLUX_HTTP
  #include <ESP8266HTTPClient.h>
  int httpResponseCode = 0;
#endif

#ifdef INFLUX_UDP
  WiFiUDP udp;
  IPAddress influxhost = {SECRET_INFLUX_IP_OCTET1, SECRET_INFLUX_IP_OCTET2, SECRET_INFLUX_IP_OCTET3, SECRET_INFLUX_IP_OCTET4}; // The IP address of the InfluxDB host for UDP packets.
  int influxport = 8089; // The port that the InfluxDB server is listening on
#endif

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
#define PANEL_VOLTS      0x00
#define PANEL_AMPS       0x01
#define PANEL_POWER_L    0x02
#define PANEL_POWER_H    0x03
#define BATT_VOLTS       0x04
#define BATT_AMPS        0x05
#define BATT_POWER_L     0x06
#define BATT_POWER_H     0x07
#define LOAD_VOLTS       0x0C
#define LOAD_AMPS        0x0D
#define LOAD_POWER_L     0x0E
#define LOAD_POWER_H     0x0F
#define BATT_TEMP        0x10
#define INT_TEMP         0x11
#define HEATSINK_TEMP    0x12
#define BATT_SOC         0x1A
#define REMOTE_BATT_TEMP 0x1B

const int realtimeMetrics = 11; //The number of realtime metrics we'll be collecting.
float realtime[realtimeMetrics];

byte avSamples = 10;
int failures = 0; //The number of failed WiFi or HTTP post attempts. Will automatically restart the ESP if too many failures occurr in a row.

RunningAverage realtimeAverage[realtimeMetrics] = {
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples),
  RunningAverage(avSamples)
};

byte collectedSamples = 0;
unsigned long lastUpdate = 0;

const char *realtimeMetricNames[] = {"PV_array_voltage",
                       "PV_array_current",
                       "PV_array_power",
                       "Battery_voltage",
                       "Battery_charging_current",
                       "Battery_charging_power",
                       "Load_voltage",
                       "Load_current",
                       "Load_power",
                       "Battery_temperature",
                       "Internal_temperature"};

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

// instantiate ModbusMaster object
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

  //Telnet log is accessible at port 23
  TelnetPrint.begin();
}


void sendInfluxData (const char *postData) {
  TelnetPrint.print("Posting to InfluxDB: "); TelnetPrint.println(postData);

  #ifdef INFLUX_UDP
    udp.beginPacket(influxhost, influxport);
    udp.printf(postData);
    udp.endPacket();
    delay(5); //This is required to allow the UDP transmission to complete
  #endif

  #ifdef INFLUX_HTTP
    WiFiClient client;
    HTTPClient http;
    http.begin(client, SECRET_INFLUXDB);
    http.addHeader("Content-Type", "text/plain");
    
    httpResponseCode = http.POST(postData);
    delay(10); //For some reason this delay is critical to the stability of the ESP.
    
    if (httpResponseCode >= 200 && httpResponseCode < 300){ //If the HTTP post was successful
      String response = http.getString(); //Get the response to the request
      //Serial.print("HTTP POST Response Body: "); Serial.println(response);
      TelnetPrint.print("HTTP POST Response Code: "); TelnetPrint.println(httpResponseCode);

      if (failures >= 1) {
        failures--; //Decrement the failure counter.
      }
    }
    else {
      TelnetPrint.print("Error sending HTTP POST: "); TelnetPrint.println(httpResponseCode);
      if (httpResponseCode <= 0) {
        failures++; //Incriment the failure counter if the server couldn't be reached.
      }
    }
    http.end();
    client.stop();
  #endif
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
  
    float rssi = WiFi.RSSI();
    Serial.print("WiFi signal strength is: "); Serial.println(rssi);
    TelnetPrint.print("WiFi signal strength is: "); TelnetPrint.println(rssi);

    TelnetPrint.println("30 seconds has passed. Reading the MODBUS...");
    uint8_t result;
    
    Serial.flush(); //Make sure the hardware serial buffer is empty before communicating over MODBUS.
    
    EPEver.clearResponseBuffer();
    result = EPEver.readInputRegisters(0x3100, 18); // Read registers starting at 0x3100 for the realtime data.
    
    if (result == EPEver.ku8MBSuccess) {
      TelnetPrint.println("MODBUS read successful.");
      for (int i = 0; i < realtimeMetrics; i++) {
        realtime[i] = 0.0; //Clear all the metrics.
        switch (i) {
          case 0: //PV array voltage
            realtime[i] = EPEver.getResponseBuffer(PANEL_VOLTS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 1: //PV array current
            realtime[i] = EPEver.getResponseBuffer(PANEL_AMPS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 2: //PV array power
            realtime[i] = (EPEver.getResponseBuffer(PANEL_POWER_L) | (EPEver.getResponseBuffer(PANEL_POWER_H) << 8))/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 3: //Battery voltage
            realtime[i] = EPEver.getResponseBuffer(BATT_VOLTS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 4: //Battery charging current
            realtime[i] = EPEver.getResponseBuffer(BATT_AMPS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 5: //Battery charging power
            realtime[i] = (EPEver.getResponseBuffer(BATT_POWER_L) | (EPEver.getResponseBuffer(BATT_POWER_H) << 8))/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 6: //Load voltage
            realtime[i] = EPEver.getResponseBuffer(LOAD_VOLTS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 7: //Load current
            realtime[i] = EPEver.getResponseBuffer(LOAD_AMPS)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 8: //Load power
            realtime[i] = (EPEver.getResponseBuffer(LOAD_POWER_L) | (EPEver.getResponseBuffer(LOAD_POWER_H) << 8))/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 9: //Battery temperature
            realtime[i] = EPEver.getResponseBuffer(BATT_TEMP)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
          case 10: //Internal temperature
            realtime[i] = EPEver.getResponseBuffer(INT_TEMP)/100.0f;
            realtimeAverage[i].addValue(realtime[i]);
            break;
        }
      
        Serial.print(realtimeMetricNames[i]); Serial.print(": "); Serial.print(realtime[i]); Serial.print(" Average: "); Serial.println(realtimeAverage[i].getAverage());

        char realtimeString[8];
        dtostrf(realtime[i], 1, 2, realtimeString);
        
        char realtimeAvString[8];
        dtostrf(realtimeAverage[i].getAverage(), 1, 2, realtimeAvString);
        
        TelnetPrint.print(realtimeMetricNames[i]); TelnetPrint.print(": "); TelnetPrint.print(realtimeString); TelnetPrint.print(" Average: "); TelnetPrint.println(realtimeAvString);

        if (collectedSamples == avSamples) { //Once we've collected enough samples to flush the averages, send the data to InfluxDB.
          TelnetPrint.println("Posting the data...");

          #if defined (INFLUX_HTTP) || defined (INFLUX_UDP)
            char post[70];
            sprintf(post, "%s,sensor=solar value=%s", realtimeMetricNames[i], realtimeAvString);
            sendInfluxData(post);
          #endif
        }
        yield();
      }

      if (collectedSamples >= avSamples) {
        TelnetPrint.print(collectedSamples); TelnetPrint.println(" samples have been collected. Resetting the counter."); TelnetPrint.println();
        collectedSamples = 0;
        TelnetPrint.println("Posting uptime data...");

        char uptimeString[20];
        dtostrf((millis()/60000), 1, 0, uptimeString); //Uptime in minutes.


        #if defined (INFLUX_HTTP) || defined (INFLUX_UDP)
          char post[70];
          sprintf(post, "uptime,sensor=solar value=%s", uptimeString);
          sendInfluxData(post);
        #endif
      }
      else {
        collectedSamples++;
        TelnetPrint.print(collectedSamples); TelnetPrint.println(" samples have been collected."); TelnetPrint.println();
      }
    }

    else {
      Serial.print("MODBUS read failed. Returned value: ");
      Serial.println(result, HEX);
      TelnetPrint.print("MODBUS read failed. Returned value: "); TelnetPrint.println(result);
      failures++;
      TelnetPrint.print("Failure counter: "); TelnetPrint.println(failures);
    }

    lastUpdate = millis();
  }

  if (failures >= 20) {
    Serial.print("Too many failures, rebooting...");
    TelnetPrint.print("Failure counter has reached: "); TelnetPrint.print(failures); TelnetPrint.println(". Rebooting...");
    ESP.restart(); //Reboot the ESP if there's been problems sending the data.
  }
}