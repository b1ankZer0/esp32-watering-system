#define BLYNK_TEMPLATE_ID "TMPL6o2ZJMmow"
#define BLYNK_TEMPLATE_NAME "project"
#define BLYNK_AUTH_TOKEN "NIYnBt8LZ3FBrjVVSOQataZzEvqUdRJx"

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <Preferences.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>

class PersistentStorage {
  private:
      Preferences prefs;
      const char* _namespace;

  public:
      PersistentStorage(const char* ns) : _namespace(ns) {}

      void set(const char* key, int value) {
          prefs.begin(_namespace, false);
          prefs.putInt(key, value);
          prefs.end();
      }

      int get(const char* key) {
          prefs.begin(_namespace, true);
          int value = prefs.getInt(key, 0);
          prefs.end();
          return value;
      }

      void update(const char* key, int newValue) {
          set(key, newValue);
      }

      void remove(const char* key) {
          prefs.begin(_namespace, false);
          prefs.remove(key);
          prefs.end();
      }

      void clearAll() {
          prefs.begin(_namespace, false);
          prefs.clear();
          prefs.end();
      }
};

class BtnClass{
  private:
    int d = 50;
    bool click(int pin) {
      int buttonState = digitalRead(pin); 
      if (buttonState == HIGH) return false;
      return true;
    }

  public:
    BtnClass() {}

    bool click1(int pin) {
      bool btnStatus = click(pin);
      delay(d);
      return btnStatus;
    }

    int clickM(int pin) {
      int c= 0;
      while(click(pin)){
        while(click(pin)){}
        delay(d);
        c++;
      }
      return c;
    }

    bool click2(int pin) {
      if(!click(pin)) return false;
      delay(200);
      if (click(pin)) {
        return true;
      }
      return false;
    }

    bool longPrass(int pin,int ms) {
      int loopDelay=10;
      if(!click(pin)) return false;
      int loop=ms/loopDelay;

      while(loop){
        delay(loopDelay);
        if (!click(pin)) {
          return false;
        }
        loop--;
      }

      return true;
    }
};

class TimerManager {
  private:
    unsigned long _waitForSec = 0;
    unsigned long _runForSec  = 0;
    unsigned long _lastTime   = 0;

  public:
    TimerManager() {}

    void setWaitForSec(unsigned long sec) { _waitForSec = sec * 1000UL; }
    void setRunForSec(unsigned long sec)  { _runForSec  = sec * 1000UL; }
    void setLastTime(unsigned long ms)    { _lastTime   = ms; }
    void markLastTime()                   { _lastTime   = millis(); }

    unsigned long getWaitForSec() const { return _waitForSec / 1000UL; }
    unsigned long getRunForSec()  const { return _runForSec  / 1000UL; }
    unsigned long getLastTime()   const { return _lastTime; }

    bool isWaitTimeOver() const {
        return (millis() - _lastTime) >= _waitForSec;
    }

    bool isRunTimeOver() {
        bool state= (millis() - _lastTime) >= (_waitForSec + _runForSec);
        if(state) markLastTime();
        return state;
    }

    unsigned long willRunAfterSec() const {
        unsigned long elapsed = millis() - _lastTime;
        if (elapsed >= _waitForSec) return 0;
        return (_waitForSec - elapsed) / 1000UL;
    }

    unsigned long willStopAfterSec() const {
        unsigned long elapsed = millis() - _lastTime;
        unsigned long total = _waitForSec + _runForSec;
        if (elapsed >= total) return 0;
        return (total - elapsed) / 1000UL;
    }
};

class SensorsManager {
    private:
        DHT _dht;
        int _soilPin;
        
        struct SensorData {
            float temp = 0;
            float humidity = 0;
            int soilMoisture = 0;
            bool isValid = false;
        } _currentData;

    public:
        SensorsManager(uint8_t dhtPin, uint8_t dhtType, int soilPin) 
            : _dht(dhtPin, dhtType), _soilPin(soilPin) {}

        void begin() {
            _dht.begin();
            pinMode(_soilPin, INPUT);
        }

        void update() {
            float h = _dht.readHumidity();
            float t = _dht.readTemperature();
            
            if (isnan(h) || isnan(t)) {
                _currentData.isValid = false;
            } else {
                _currentData.temp = t;
                _currentData.humidity = h;
                _currentData.isValid = true;
            }

            int rawSoil = analogRead(_soilPin);
            _currentData.soilMoisture = map(rawSoil, 4095, 0, 0, 100);
        }

        int getTemp() { return (int)_currentData.temp; }
        int getHumidity() { return (int)_currentData.humidity; }
        int getSoil() { return _currentData.soilMoisture; }
        bool isReady() { return _currentData.isValid; }
};

class DisplayManager {
  private:
    U8G2_SH1106_128X64_NONAME_F_HW_I2C* _u8g2;
    bool _isAsleep = false;
    const uint8_t* sSize = u8g2_font_6x10_tf;
    const uint8_t* mSize = u8g2_font_helvB10_tr;
    const uint8_t* lSize = u8g2_font_helvB12_tr;

    struct RenderState {
        char  menuTitle[64] = "";
        int   temp  = INT_MIN;
        int   hum   = INT_MIN;
        int   soil   = INT_MIN;
        bool  wifi  = false;
        bool  ble   = false;
        bool  valid = false;
    } _lastState;

    struct IconDef {
        const uint8_t* font;
        uint16_t glyph;
    };

    struct Icons {
        IconDef bt   = {u8g2_font_open_iconic_embedded_1x_t, 0x004b};
        IconDef wifi = {u8g2_font_open_iconic_embedded_1x_t, 0x0050};
        IconDef humu = {u8g2_font_open_iconic_thing_1x_t,    0x0048};
    } _icons;

    bool stateChanged(const char* menuTitle, int temp, int hum, int soil, bool wifi, bool ble) {
        if (!_lastState.valid)                                                                 return true;
        if (_lastState.temp != temp)                                                           return true;
        if (_lastState.hum  != hum)                                                            return true;
        if (_lastState.soil  != soil)                                                          return true;
        if (_lastState.wifi != wifi)                                                           return true;
        if (_lastState.ble  != ble)                                                            return true;
        if (strncmp(_lastState.menuTitle, menuTitle, sizeof(_lastState.menuTitle)) != 0) return true;
        return false;
    }

    void saveState(const char* menuTitle, int temp, int hum, int soil, bool wifi, bool ble) {
        strncpy(_lastState.menuTitle, menuTitle, sizeof(_lastState.menuTitle) - 1);
        _lastState.menuTitle[sizeof(_lastState.menuTitle) - 1] = '\0';
        _lastState.temp  = temp;
        _lastState.hum   = hum;
        _lastState.soil   = soil;
        _lastState.wifi  = wifi;
        _lastState.ble   = ble;
        _lastState.valid = true;
    }

    void drawIcon(u8g2_uint_t x, u8g2_uint_t y, String name) {
        IconDef selected;

        if (name == "wifi")      selected = _icons.wifi;
        else if (name == "bt")   selected = _icons.bt;
        else if (name == "humu") selected = _icons.humu;
        else return;

        _u8g2->setFont(selected.font);
        _u8g2->drawGlyph(x, y, selected.glyph);
    }

    void drawText(u8g2_uint_t x, u8g2_uint_t y, String text, int size) {
        const uint8_t* selectedFont;

        switch(size) {
            case 1:  selectedFont = sSize; break;
            case 2:  selectedFont = mSize; break;
            case 3:  selectedFont = lSize; break;
            default: selectedFont = sSize; break;
        }

        _u8g2->setFont(selectedFont);
        _u8g2->drawStr(x, y, text.c_str());
    }

    void drawTopBar(bool wifi, bool ble, int temp, int hum,int soil) {
        if (wifi) drawIcon(0, 10, "wifi");
        if (ble)  drawIcon(15, 10, "bt");

        drawText(45, 9, "s"+String(soil), 1);
        drawText(70, 9, "t"+String(temp), 1);

        drawIcon(100, 10, "humu");
        drawText(110, 9, String(hum), 1);

        _u8g2->drawHLine(0, 12, 128);
    }

    void drawBottomBar(int mod) {
        _u8g2->drawHLine(0, 52, 128);
        _u8g2->setFont(u8g2_font_helvB10_tr);
        if(mod==0){
            _u8g2->drawStr(115, 63, ">");
        }else if(mod==2){
            _u8g2->drawStr(5, 63, "<");
            _u8g2->drawStr(55, 63, "OK");
            _u8g2->drawStr(115, 63, ">");
        }else if(mod==1){
            _u8g2->drawStr(5, 63, "n/2");
            _u8g2->drawStr(55, 63, "OK");
            _u8g2->drawStr(100, 63, "n*2");
        }
    }

  public:
    DisplayManager(U8G2_SH1106_128X64_NONAME_F_HW_I2C* u8g2) : _u8g2(u8g2) {}

    void begin() {
        _u8g2->begin();
    }

    void setSleep(bool sleep) {
        _isAsleep = sleep;
        _u8g2->setPowerSave(sleep ? 1 : 0);
        if (!sleep) _lastState.valid = false;
    }

    void swtchSleepState() {
        _isAsleep = !_isAsleep;
        _u8g2->setPowerSave(_isAsleep ? 1 : 0);
        if (!_isAsleep) _lastState.valid = false;
    }

    void forceRender() {
        _lastState.valid = false;
    }

    void render(const char* menuTitle, int temp, int hum, int soil, bool wifi, bool ble,int bottom) {
        if (_isAsleep) return;
        if (!stateChanged(menuTitle, temp, hum, soil, wifi, ble)) return;

        _u8g2->clearBuffer();

        drawTopBar(wifi, ble, temp, hum, soil);

        _u8g2->setFont(u8g2_font_helvB12_tr);
        int width = _u8g2->getStrWidth(menuTitle);
        _u8g2->drawStr((128 - width) / 2, 35, menuTitle);

        drawBottomBar(bottom);

        _u8g2->sendBuffer();

        saveState(menuTitle, temp, hum, soil, wifi, ble);
    }
};

BlynkTimer timer;

char ssid[] = "Bhuiyan";
char pass[] = "Alfa@0155";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
PersistentStorage userSettings("config");
DisplayManager display(&u8g2);
BtnClass buttons;
SensorsManager sensors(4, DHT11, 5);
TimerManager pumpTimer;
int sPin=6;
int pPin=7;
int okPin=15;
int nPin=16;
int alarmPin=17;
int relaPin=18;
int blynkPin=12;
int mod=0;
int seq=0;
int oOff=0;
int started=0;
bool isOn=false;
unsigned long vlu=0;

// void onRelaState();
// void offRelaState();

void sendSensorData() {
    if (sensors.isReady()) {
        float soilHum = sensors.getSoil();
        Blynk.virtualWrite(V0, sensors.getTemp());
        Blynk.virtualWrite(V1, sensors.getHumidity());
        Blynk.virtualWrite(V2, soilHum);
    }
}

void setup() {
    pinMode(sPin, INPUT_PULLUP);
    pinMode(pPin, INPUT_PULLUP);
    pinMode(okPin, INPUT_PULLUP);
    pinMode(nPin, INPUT_PULLUP);
    pinMode(alarmPin, OUTPUT);
    pinMode(relaPin, OUTPUT);

    onRelaState();
    offRelaState();
    pumpTimer.setWaitForSec(10);
    pumpTimer.setRunForSec(1);
    pumpTimer.markLastTime();

    Wire.begin(11, 10);
    Serial.begin(9600);
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    display.begin();
    sensors.begin();
    timer.setInterval(100L, sendSensorData);
}

void loop() {
    // digitalWrite(blynkPin,LOW);
    Blynk.run();
    timer.run();
    sensors.update();

    String title = "";
    bool isOnlineOn=!buttons.click1(blynkPin);

    if (buttons.click1(sPin)) {
        seq=0;
        mod=0;
        offRelaState();
        display.swtchSleepState();
    }
    else if (buttons.click1(pPin)) {
        if(mod==1){
            vlu=vlu/2;
            if(vlu<1) vlu=1;
        }
        offRelaState();
    } 
    else if (buttons.click1(okPin)) { 
        if(seq==0){
            pumpTimer.setWaitForSec(vlu);  
            vlu=pumpTimer.getRunForSec();
            seq=1;    
        }else if(seq==1){
            pumpTimer.setRunForSec(vlu);
            seq=0;
            mod=0;
        }
        offRelaState();
    } 
    else if (buttons.click1(nPin)) {
        if(mod==1){
            vlu=vlu*2;
        }else if(mod==0){
            mod=1;
            vlu=pumpTimer.getWaitForSec();
        }
        offRelaState();
    }
    else if (isOnlineOn) {
        oOff=0;
        onRelaState();
        title = "wifi by On!";
    } 
    else if(oOff=0 && !isOnlineOn){
        started=1;
        oOff=1;
        title = "wifi by Off!";
        offRelaState();
    }

    if (mod==1 && seq==0) {
        title = "WaitSec: " + String(vlu) + "s";
    } 
    else if(mod==1 && seq==1){
        title = "RunSec: " + String(vlu) + "s";
    }


    if (!isOnlineOn && !title.length() && !pumpTimer.isWaitTimeOver()) {
        title = "Pump in: " + String(pumpTimer.willRunAfterSec()) + "s";
        offRelaState();
    } else if (!isOnlineOn && !title.length() && !pumpTimer.isRunTimeOver()) {
        title = "Stops in: " + String(pumpTimer.willStopAfterSec()) + "s";
        onRelaState();
    }

    int soilHum = sensors.getSoil();
    if(soilHum > 60){
        offRelaState();
    }
    
    if (!sensors.isReady()) {
        display.render("SENSOR ERR", 0, 0, 0, false, false, 0);
    } else {
        bool isConnected = (WiFi.status() == WL_CONNECTED);
        display.render(
            title.c_str(), 
            sensors.getTemp(), 
            sensors.getHumidity(), 
            soilHum,
            isConnected, 
            false,
            mod
        );
    }
}

void onRelaState(){
    if(!isOn){
        digitalWrite(relaPin, LOW);
        digitalWrite(alarmPin,HIGH);
        isOn=true;
    }
}

void offRelaState(){
    if(isOn){
        digitalWrite(relaPin, HIGH);
        digitalWrite(alarmPin,LOW);
        isOn=false;
    }
}
