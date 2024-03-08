// Include required libraries
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "PubSubClient.h"
#include <SPIFFSIniFile.h>
#include "FS.h"

inline const char* read_Config(const char* section, const char* key);
void callback(const char *topic, byte *payload, unsigned int length);
void printErrorMessage(uint8_t e, bool eol = true);

inline void goToDeepSleep(long long sleepTime)
{
  // Configure the timer to wake us up! Time in uS
  esp_sleep_enable_timer_wakeup(sleepTime * 60LL * 1000000LL);
  esp_deep_sleep_start();
}

// Structure Config with default configuration parameters
struct Config{
  // I2S          // TODO: on board pin names 
  protected:
    int8_t I2S_Dout = 22; // DIN
    int8_t I2S_Blck = 26; // BCK
    int8_t I2S_Lrc = 25;  // LCK
  private:
    //Wi-Fi
    const char *WiFi_ssid;
    const char *WiFi_password;
    const //MQTT
    const char *MQTT_broker;
    const char *MQTT_topicRinging;
    const char *MQTT_topicSleep;
    const char *MQTT_topicAdvertiseUnit;
    const char *MQTT_topicUnit;
    const char *MQTT_topicControl;
    const char *MQTT_username;
    const char *MQTT_password;
    int MQTT_port;
    // Path to song
    String SONG_URL; 
    // Audio
    int AUDIO_volume = 25;
    // *during night
    //int AUDIO_volume = 2;
    // Serial monitor boudrate
    int SERIAL_baudrate = 115200;

  public:
    inline int8_t get_I2S_Dout(){
      return I2S_Dout;
    }
    inline int8_t get_I2S_Blck(){
      return I2S_Blck;
    }
    inline int8_t get_I2S_Lrc(){
      return I2S_Lrc;
    }
    // WiFi configuration
    inline const char* get_WiFi_ssid(){
      return WiFi_ssid;
    }
    inline const char* get_WiFi_password(){
      return WiFi_password;
    }
    // MQTT Configuration
    inline const char* get_MQTT_Broker() {
        return MQTT_broker;
    }
    inline const char* get_MQTT_Topic_Ringing() {
        return MQTT_topicRinging;
    }
    inline const char* get_MQTT_Topic_Sleep() {
        return MQTT_topicSleep;
    }
    inline const char* get_MQTT_Topic_Advertise_Unit() {
        return MQTT_topicAdvertiseUnit;
    }
    inline const char* get_MQTT_Topic_Unit() {
        return MQTT_topicUnit;
    }
    inline const char* get_MQTT_Topic_Control() {
        return MQTT_topicControl;
    }
    inline const char* get_MQTT_Username() {
        return MQTT_username;
    }
    inline const char* get_MQTT_Password() {
        return MQTT_password;
    }
    inline int get_MQTT_Port() {
        return MQTT_port;
    }
    // Song URL Configuration
    inline String get_Song_URL() {
        return SONG_URL;
    }
    // Audio Configuration
    inline int get_Audio_Volume() {
        return AUDIO_volume;
    }
    // Serial Configuration
    inline int get_Serial_Baudrate() {
        return SERIAL_baudrate;
    }

    // WiFi Configuration
    inline void setWiFiSSID() {
        WiFi_ssid = read_Config("WiFi", "ssid");
    }

    inline void setWiFiPassword() {
        WiFi_password = read_Config("WiFi", "password");
    }

    // MQTT Configuration
    inline void setMQTTBroker() {
        MQTT_broker = read_Config("MQTT", "broker");
    }

    inline void setMQTTTopicRinging() {
        MQTT_topicRinging = read_Config("MQTT", "topicRinging");
    }

    inline void setMQTTTopicSleep() {
        MQTT_topicSleep = read_Config("MQTT", "topisSleep");
    }

    inline void setMQTTTopicAdvertiseUnit() {
        MQTT_topicAdvertiseUnit = read_Config("MQTT", "topiecAdvertiseUnit");
    }

    inline void setMQTTTopicUnit() {
        MQTT_topicUnit = read_Config("MQTT", "topicControl");
    }

    inline void setMQTTTopicControl() {
        MQTT_topicControl = read_Config("MQTT", "topicControl");
    }

    inline void setMQTTUsername() {
        MQTT_username = read_Config("MQTT", "username");
    }

    inline void setMQTTPassword() {
        MQTT_password = read_Config("MQTT", "password");
    }

    inline void setMQTTPort() {
        MQTT_port = atoi(read_Config("MQTT", "port"));
    }

    // Song URL Configuration
    inline void setSongURL() {
        SONG_URL = read_Config("URL", "song");
    }

};

// Create file object for reading configuration.ini
const size_t bufferLen = 80;
char buffer[bufferLen];
const char *file = "/conf.ini";
SPIFFSIniFile ini(file);

// Create config object
Config config;
// Create audio object
Audio audio;
// Create WI-Fi client object and MQTT client object
WiFiClient espClient;
PubSubClient client(espClient);

// SETUP // * ///////////////////////////////////////////////////////////////////
void setup() {
  // Start Serial Monitor
  Serial.begin(config.get_Serial_Baudrate());

  // Setup WiFi in Station mode
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.get_WiFi_ssid(), config.get_WiFi_password());

  for (int i = 0; WiFi.status() != WL_CONNECTED; i++) {
    delay(500);
    if (i % 100 == 0) {
      Serial.println();
    }
    Serial.print(".");
  }

  // Wi-Fi Connected, print IP to serial monitor
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("");

  //connecting to a MQTT broker
  client.setServer(config.get_MQTT_Broker(), config.get_MQTT_Port());
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), config.get_MQTT_Username(), config.get_MQTT_Password())) {
      Serial.println("Private mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  
  client.publish(config.get_MQTT_Topic_Advertise_Unit(), WiFi.macAddress().c_str());

  client.subscribe(config.get_MQTT_Topic_Ringing());
  client.subscribe(config.get_MQTT_Topic_Sleep());
  client.subscribe(config.get_MQTT_Topic_Control());
  client.subscribe(WiFi.macAddress().c_str()); 

  //Audio settings
  // Connect MAX98357 I2S Amplifier Module
  audio.setPinout(config.get_I2S_Blck(), config.get_I2S_Lrc(), config.get_I2S_Dout());
  // Set thevolume (0-100)
  audio.setVolume(config.get_Audio_Volume());

  //Mount the SPIFFS  
  if (!SPIFFS.begin())
    while (1)
      Serial.println("SPIFFS.begin() failed");
  
  if (!ini.open()) {
    Serial.print("configuration file ");
    Serial.print(file);
    Serial.println(" does not exist");
    // Cannot do anything else
    while (1)
      ;
  }
  Serial.println("configuration file exists");
  if (!ini.validate(buffer, bufferLen)) {
    Serial.print("Ini file ");
    Serial.print(ini.getFilename());
    Serial.print(" not valid: ");
    printErrorMessage(ini.getError());
    // Cannot do anything else
    while (1)
      ;
  }
}

void loop() {
  client.loop();
  audio.loop();
  if (audio.getAudioCurrentTime() >= 20){
    audio.setVolume(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 20));
    Serial.println(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 20)); // audio fade out
    if(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 20) <= 1){
      audio.stopSong();
      audio.connecttohost("none");
      Serial.println("stopped");
    }
  }
}

inline const char* read_Config(const char* section, const char* key){
  ini.getValue(section, key, buffer, bufferLen);
  return buffer;
}

void callback(const char *topic, byte *payload, unsigned int length) {
  String sTopic = topic;
  Serial.println(sTopic);

  // * globálne riadenie
  if (sTopic == config.get_MQTT_Topic_Control()){
    String command = "";
    for (int i = 0; i < length; i++) {
      command += (char) payload[i];
    }
    if (command == "here"){
      Serial.println("here");
      client.publish(config.get_MQTT_Topic_Advertise_Unit(), WiFi.macAddress().c_str());
    }
  }

  // * funkcia na uspatie esp32
  else if (sTopic == config.get_MQTT_Topic_Sleep()) {
    String time_to_sleep = "";
    for (int i = 0; i < length; i++) {
      time_to_sleep += (char) payload[i];
    }
    Serial.println(time_to_sleep);
    goToDeepSleep((long long)time_to_sleep.toInt());
  }

  // * funkcia na zapnutie odberu audia
  // TODO: dočasne daktivované
  else if (sTopic == config.get_MQTT_Topic_Ringing()) {
    String SONG_ID = "";
    for (int i = 0; i < length; i++) {
      SONG_ID += (char) payload[i];
    }
    String SONG_path = config.get_Song_URL() + SONG_ID;
    // Pripája sa
    int fails = 0;
    while (!audio.isRunning()) {
      delay(10);
      fails++;
      audio.connecttohost(SONG_path.c_str());
      if(fails == 10) {
        break;
      } 
    }
    audio.setVolume(config.get_Audio_Volume());

    // Run audio player
    audio.loop();
    // kontrolný výpis
    if (!audio.isRunning()) {
      Serial.println("koniec zvonenia");
    }
  }

  else if (sTopic == WiFi.macAddress().c_str()){
    String controlPARAM = "";
    for (int i = 0; i < length; i++) {
      controlPARAM += (char) payload[i];
    }
    Serial.println(controlPARAM);
  }
}

void printErrorMessage(uint8_t e, bool eol = true)
{
  switch (e) {
  case SPIFFSIniFile::errorNoError:
    Serial.print("no error");
    break;
  case SPIFFSIniFile::errorFileNotFound:
    Serial.print("file not found");
    break;
  case SPIFFSIniFile::errorFileNotOpen:
    Serial.print("file not open");
    break;
  case SPIFFSIniFile::errorBufferTooSmall:
    Serial.print("buffer too small");
    break;
  case SPIFFSIniFile::errorSeekError:
    Serial.print("seek error");
    break;
  case SPIFFSIniFile::errorSectionNotFound:
    Serial.print("section not found");
    break;
  case SPIFFSIniFile::errorKeyNotFound:
    Serial.print("key not found");
    break;
  case SPIFFSIniFile::errorEndOfFile:
    Serial.print("end of file");
    break;
  case SPIFFSIniFile::errorUnknownError:
    Serial.print("unknown error");
    break;
  default:
    Serial.print("unknown error value");
    break;
  }
  if (eol)
    Serial.println();
}

void audio_info(const char *info) {
  Serial.print("info        ");
  Serial.println(info);
}
void audio_id3data(const char *info) {  //id3 metadata
  Serial.print("id3data     ");
  Serial.println(info);
}
void audio_eof_mp3(const char *info) {  //end of file
  Serial.print("eof_mp3     ");
  Serial.println(info);
}
void audio_showstation(const char *info) {
  Serial.print("station     ");
  Serial.println(info);
}
void audio_showstreaminfo(const char *info) {
  Serial.print("streaminfo  ");
  Serial.println(info);
}
void audio_showstreamtitle(const char *info) {
  Serial.print("streamtitle ");
  Serial.println(info);
}
void audio_bitrate(const char *info) {
  Serial.print("bitrate     ");
  Serial.println(info);
}
void audio_commercial(const char *info) {  //duration in sec
  Serial.print("commercial  ");
  Serial.println(info);
}
void audio_icyurl(const char *info) {  //homepage
  Serial.print("icyurl      ");
  Serial.println(info);
}
void audio_lasthost(const char *info) {  //stream URL played
  Serial.print("lasthost    ");
  Serial.println(info);
}
void audio_eof_speech(const char *info) {
  Serial.print("eof_speech  ");
  Serial.println(info);
}