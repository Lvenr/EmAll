#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

class SerialCommander {
public:
    SerialCommander(unsigned long baudRate = 115200) 
        : _baudRate(baudRate),
          _lastDisplayUpdate(0),
          _displayUpdateInterval(1000),
          _autoDisplayEnabled(true),
          _lastIdleTime(0),
          _lastWorkTime(0),
          _cpuLoad(0.0),
          _freeRam(0),
          _u8g2(nullptr) {
        for (int i = 0; i < 20; i++) {
            cpuLoadHistory[i] = 0;
        }
        metricsIndex = 0;
    }
    void begin(uint8_t sdaPin = SDA, uint8_t sclPin = SCL) {
        Serial.begin(_baudRate);
        while (!Serial) {
            delay(10);
        }

        _initOLED(sdaPin, sclPin);

        _lastIdleTime = millis();
        _lastWorkTime = _lastIdleTime;
        
        Serial.println(F("=== SerialCommander v1.0 ==="));
        Serial.println(F("System monitor with OLED display"));
        Serial.print(F("Baud rate: ")); Serial.println(_baudRate);
        Serial.println(F("Commands: info, clear, help, monitor [on/off]"));
        Serial.println(F("============================="));
        
        showMessage("System", "Monitor", "v1.0", "Ready");
    }
    

    void update() {
       
        _handleSerialCommands();
        
   
        if (_autoDisplayEnabled && _u8g2) {
            unsigned long currentTime = millis();
            if (currentTime - _lastDisplayUpdate >= _displayUpdateInterval) {
                _updateSystemMetrics();
                _displaySystemInfo();
                _lastDisplayUpdate = currentTime;
            }
        }
    }
    
   
    void showMessage(const String& line1, const String& line2 = "",
                    const String& line3 = "", const String& line4 = "") {
        if (!_u8g2) return;
        
        _u8g2->clearBuffer();
        
        uint8_t yPos = 0;
        if (line1.length() > 0) {
            _u8g2->drawStr(0, yPos, line1.c_str());
            yPos += 12;
        }
        if (line2.length() > 0) {
            _u8g2->drawStr(0, yPos, line2.c_str());
            yPos += 12;
        }
        if (line3.length() > 0) {
            _u8g2->drawStr(0, yPos, line3.c_str());
            yPos += 12;
        }
        if (line4.length() > 0) {
            _u8g2->drawStr(0, yPos, line4.c_str());
        }
        
        _u8g2->sendBuffer();
    }
    
  
    void clearDisplay() {
        if (_u8g2) {
            _u8g2->clearBuffer();
            _u8g2->sendBuffer();
        }
    }
    
  
    void setUpdateInterval(unsigned long intervalMs) {
        _displayUpdateInterval = intervalMs;
        Serial.print(F("Update interval set to "));
        Serial.print(intervalMs);
        Serial.println(F(" ms"));
    }
    

    void enableAutoDisplay(bool enable) {
        _autoDisplayEnabled = enable;
        Serial.print(F("Auto display "));
        Serial.println(enable ? "enabled" : "disabled");
    }
    
   
    float getCpuLoad() { return _cpuLoad; }
    
  
    uint32_t getFreeMemory() { return _freeRam; }
    
  
    uint32_t getTotalMemory() {
        #ifdef __AVR_ATmega328P__
        return 2048; 
        #elif defined(__AVR_ATmega2560__)
        return 8192; 
        #elif defined(ESP8266)
        return 81920; 
        #elif defined(ESP32)
        return 327680; 
        #else
        return 8192; 
        #endif
    }
    
  
    uint32_t getUsedMemory() {
        return getTotalMemory() - _freeRam;
    }
   
    unsigned long getUptime() {
        return millis() / 1000;
    }

private:
    unsigned long _baudRate;
    unsigned long _lastDisplayUpdate;
    unsigned long _displayUpdateInterval;
    bool _autoDisplayEnabled;
    

    unsigned long _lastIdleTime;
    unsigned long _lastWorkTime;
    float _cpuLoad;
    uint32_t _freeRam;
    

    float cpuLoadHistory[20];
    uint8_t metricsIndex;
    
  
    U8G2_SSD1306_128X64_NONAME_F_SW_I2C* _u8g2;
    
  
    void _initOLED(uint8_t sdaPin, uint8_t sclPin) {

        Wire.begin(sdaPin, sclPin);
        

        _u8g2 = new U8G2_SSD1306_128X64_NONAME_F_SW_I2C(U8G2_R0, sclPin, sdaPin, U8X8_PIN_NONE);
        
        if (_u8g2) {
            _u8g2->begin();
            _u8g2->clearBuffer();
            _u8g2->setFont(u8g2_font_6x10_tf);
            _u8g2->setFontRefHeightExtendedText();
            _u8g2->setDrawColor(1);
            _u8g2->setFontPosTop();
            _u8g2->setFontDirection(0);
            
            Serial.println(F("OLED: SSD1306 128x64 initialized"));
        } else {
            Serial.println(F("ERROR: Failed to initialize OLED"));
        }
    }
    
    void _handleSerialCommands() {
        if (Serial.available()) {
            String cmd = Serial.readStringUntil('\n');
            cmd.trim();
            
            if (cmd.length() > 0) {
                Serial.print(F("> "));
                Serial.println(cmd);
                _processCommand(cmd);
            }
        }
    }
    
    void _processCommand(const String& cmd) {
        if (cmd == "info" || cmd == "status") {
            _updateSystemMetrics();
            _printSystemInfo();
            showMessage("CPU: " + String(_cpuLoad, 1) + "%", 
                       "RAM: " + String(_freeRam) + " free",
                       "Uptime: " + String(getUptime()) + "s",
                       "");
            
        } else if (cmd == "clear") {
            clearDisplay();
            Serial.println(F("Display cleared"));
            
        } else if (cmd == "help" || cmd == "?") {
            _printHelp();
            
        } else if (cmd.startsWith("monitor ")) {
            String arg = cmd.substring(8);
            if (arg == "on") {
                enableAutoDisplay(true);
            } else if (arg == "off") {
                enableAutoDisplay(false);
            } else {
                Serial.println(F("Usage: monitor [on|off]"));
            }
            
        } else if (cmd.startsWith("interval ")) {
            unsigned long interval = cmd.substring(9).toInt();
            if (interval >= 100 && interval <= 10000) {
                setUpdateInterval(interval);
            } else {
                Serial.println(F("Interval must be 100-10000 ms"));
            }
            
        } else if (cmd.startsWith("msg ")) {
            String message = cmd.substring(4);
            int space1 = message.indexOf(' ');
            int space2 = message.indexOf(' ', space1 + 1);
            int space3 = message.indexOf(' ', space2 + 1);
            
            String line1 = message.substring(0, space1);
            String line2 = (space1 > 0) ? message.substring(space1 + 1, space2) : "";
            String line3 = (space2 > 0) ? message.substring(space2 + 1, space3) : "";
            String line4 = (space3 > 0) ? message.substring(space3 + 1) : "";
            
            showMessage(line1, line2, line3, line4);
            Serial.println(F("Message displayed"));
            
        } else if (cmd == "graph") {
            _showCpuGraph();
            
        } else if (cmd == "test") {
            _runSystemTest();
            
        } else {
            Serial.print(F("Unknown command: "));
            Serial.println(cmd);
            Serial.println(F("Type 'help' for available commands"));
        }
    }
    
    void _printSystemInfo() {
        Serial.println(F("\n=== System Information ==="));
        Serial.print(F("CPU Load:    ")); Serial.print(_cpuLoad, 1); Serial.println(F("%"));
        Serial.print(F("Free RAM:    ")); Serial.print(_freeRam); Serial.println(F(" bytes"));
        Serial.print(F("Used RAM:    ")); Serial.print(getUsedMemory()); Serial.println(F(" bytes"));
        Serial.print(F("Total RAM:   ")); Serial.print(getTotalMemory()); Serial.println(F(" bytes"));
        Serial.print(F("Uptime:      ")); Serial.print(getUptime()); Serial.println(F(" seconds"));
        Serial.print(F("Auto update: ")); Serial.println(_autoDisplayEnabled ? "ON" : "OFF");
        Serial.print(F("Interval:    ")); Serial.print(_displayUpdateInterval); Serial.println(F(" ms"));
        Serial.println(F("==========================\n"));
    }
    
    void _printHelp() {
        Serial.println(F("\n=== Available Commands ==="));
        Serial.println(F("info/status   - Show system information"));
        Serial.println(F("clear         - Clear OLED display"));
        Serial.println(F("help/?        - Show this help"));
        Serial.println(F("monitor on/off- Toggle auto system monitor"));
        Serial.println(F("interval N    - Set update interval (ms)"));
        Serial.println(F("msg line1 [line2...] - Show custom message"));
        Serial.println(F("graph         - Show CPU load history graph"));
        Serial.println(F("test          - Run system test"));
        Serial.println(F("==========================\n"));
    }
    
    void _updateSystemMetrics() {

        unsigned long currentTime = millis();
        unsigned long workTime = currentTime - _lastWorkTime;
        unsigned long idleTime = currentTime - _lastIdleTime;
        
        if (workTime + idleTime > 0) {
            _cpuLoad = (workTime * 100.0) / (workTime + idleTime);
            _cpuLoad = constrain(_cpuLoad, 0.0, 100.0);
        }
        

        cpuLoadHistory[metricsIndex] = _cpuLoad;
        metricsIndex = (metricsIndex + 1) % 20;
        

        #ifdef __AVR__
        extern int __heap_start, *__brkval;
        int v;
        _freeRam = (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
        #elif defined(ESP8266) || defined(ESP32)
        _freeRam = ESP.getFreeHeap();
        #else
        _freeRam = 0;
        #endif
        
        _lastWorkTime = currentTime;
        _lastIdleTime = currentTime;
    }
    
    void _displaySystemInfo() {
        if (!_u8g2) return;
        
        _u8g2->clearBuffer();
        

        _u8g2->setFont(u8g2_font_7x14_tf);
        _u8g2->drawStr(0, 0, "System Monitor");

        _u8g2->setFont(u8g2_font_6x10_tf);
        String cpuStr = "CPU: " + String(_cpuLoad, 1) + "%";
        _u8g2->drawStr(0, 18, cpuStr.c_str());
        

        _drawCpuGraph(45, 16, 80, 15);
        

        uint32_t totalMem = getTotalMemory();
        float memPercent = ((float)(totalMem - _freeRam) / totalMem) * 100.0;
        String memStr = "RAM: " + String((int)memPercent) + "% (" + 
                       String(_freeRam) + " free)";
        _u8g2->drawStr(0, 34, memStr.c_str());
        

        _drawProgressBar(0, 46, 128, 6, memPercent);
  
        String uptimeStr = "Up: " + String(getUptime()) + "s";
        _u8g2->drawStr(0, 56, uptimeStr.c_str());
        
        if (_autoDisplayEnabled) {
            _u8g2->drawStr(70, 56, "[AUTO]");
        }
        
        _u8g2->sendBuffer();
    }
    
    void _drawCpuGraph(uint8_t x, uint8_t y, uint8_t width, uint8_t height) {

        _u8g2->drawFrame(x, y, width, height);
        

        for (uint8_t i = 0; i < 19; i++) {
            uint8_t idx1 = (metricsIndex + i) % 20;
            uint8_t idx2 = (metricsIndex + i + 1) % 20;
            
            uint8_t x1 = x + (i * width / 19);
            uint8_t x2 = x + ((i + 1) * width / 19);
            uint8_t y1 = y + height - (cpuLoadHistory[idx1] * height / 100.0);
            uint8_t y2 = y + height - (cpuLoadHistory[idx2] * height / 100.0);
            
            _u8g2->drawLine(x1, y1, x2, y2);
        }
    }
    
    void _drawProgressBar(uint8_t x, uint8_t y, uint8_t width, uint8_t height, float percent) {

        _u8g2->drawFrame(x, y, width, height);
        

        uint8_t fillWidth = (percent * (width - 2)) / 100.0;
        _u8g2->drawBox(x + 1, y + 1, fillWidth, height - 2);
    }
    
    void _showCpuGraph() {
        if (!_u8g2) return;
        
        _u8g2->clearBuffer();
        
 
        _u8g2->setFont(u8g2_font_7x14_tf);
        _u8g2->drawStr(20, 0, "CPU Load History");
        
 
        _u8g2->drawFrame(10, 15, 108, 40);
        
  
        _u8g2->drawLine(10, 55, 118, 55); 
        _u8g2->drawLine(10, 15, 10, 55);  
        

        for (uint8_t i = 0; i < 19; i++) {
            uint8_t idx1 = (metricsIndex + i) % 20;
            uint8_t idx2 = (metricsIndex + i + 1) % 20;
            
            uint8_t x1 = 10 + (i * 108 / 19);
            uint8_t x2 = 10 + ((i + 1) * 108 / 19);
            uint8_t y1 = 55 - (cpuLoadHistory[idx1] * 40 / 100.0);
            uint8_t y2 = 55 - (cpuLoadHistory[idx2] * 40 / 100.0);
            
            _u8g2->drawLine(x1, y1, x2, y2);
            
  
            _u8g2->drawDisc(x1, y1, 1);
        }
        
  
        _u8g2->setFont(u8g2_font_5x8_tf);
        _u8g2->drawStr(0, 58, "0%");
        _u8g2->drawStr(0, 18, "100%");
        _u8g2->drawStr(120, 58, "20s");
        
        _u8g2->sendBuffer();
        Serial.println(F("Showing CPU load history graph"));
    }
    
    void _runSystemTest() {
        Serial.println(F("Running system test..."));
        showMessage("System", "Test", "Running", "");
        

        unsigned long startTime = millis();
        volatile int testValue = 0;
        for (long i = 0; i < 10000; i++) {
            testValue += i * i;
        }
        unsigned long cpuTime = millis() - startTime;
        
  
        _updateSystemMetrics();
        
        Serial.println(F("=== System Test Results ==="));
        Serial.print(F("CPU Test: ")); Serial.print(cpuTime); Serial.println(F(" ms"));
        Serial.print(F("Current CPU Load: ")); Serial.print(_cpuLoad, 1); Serial.println(F("%"));
        Serial.print(F("Free Memory: ")); Serial.print(_freeRam); Serial.println(F(" bytes"));
        Serial.println(F("==========================="));
        
        showMessage("Test Complete", 
                   "CPU: " + String(cpuTime) + "ms", 
                   "Load: " + String(_cpuLoad, 1) + "%",
                   "RAM: " + String(_freeRam) + "B");
    }
};