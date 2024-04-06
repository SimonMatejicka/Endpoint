// Include required libraries
#include "SPIFFS.h"
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "PubSubClient.h"
#include <SPIFFSIniFile.h>
#include "FS.h"

// Create file object for reading network.ini
const size_t bufferLen = 80;
char buffer[bufferLen];
const char* file = "/network.ini";
SPIFFSIniFile ini(file);
int top = 0;

String stats = "none :/";

void mqtt_channals();
void reconect_WiFi();
void reconect_MQTT();
void callback(const char *topic, byte *payload, unsigned int length);
inline char* read_Config(const char* section, const char* key);
String diagnose(String mac);
void printErrorMessage(uint8_t e, bool eol = true);

 void goToDeepSleep(long long sleepTime)
{
  // Configure the timer to wake us up! Time in uS
  esp_sleep_enable_timer_wakeup(sleepTime * 60LL * 1000000LL);
  esp_deep_sleep_start();
}

// Structure Config with default configuration parameters
struct Config{
  // I2S          // TODO: on board pin names 
  protected:
    // esp-wroom-32
    // int8_t I2S_Dout = 22; // DIN
    // esp-32s 
    int8_t I2S_Dout = 3; // DIN
    int8_t I2S_Blck = 26; // BCK
    int8_t I2S_Lrc = 25;  // LCK

  private:
    //Wi-Fi
    String WiFi_ssid;
    String WiFi_password;
    //MQTT
    String MQTT_broker;
    String MQTT_topicRinging;
    String MQTT_topicSleep;
    String MQTT_topicAdvertiseUnit;
    String MQTT_topicUnit;
    String MQTT_topicControl;
    String MQTT_topicCall;
    String MQTT_username;
    String MQTT_password;
    String MQTT_client_id = "esp32-";
    int MQTT_port;
    // Path to song
    String SONG_URL; 
    // Audio
    
    int AUDIO_volume = 0;
    // Serial monitor boudrate
    int SERIAL_baudrate = 115200;

  public:
    // WiFi Configuration
    void set_WiFi_SSID() {
      WiFi_ssid = read_Config("WiFi", "ssid");
    }
    void set_WiFi_Password() {
      WiFi_password = read_Config("WiFi", "password");
    }

    // MQTT Configuration
    void set_MQTT_Broker() {
      MQTT_broker = read_Config("MQTT", "broker");
    }
    void set_MQTT_Topic_Ringing() {
      MQTT_topicRinging = read_Config("MQTT", "topicRinging");
    }
    void set_MQTT_Topic_Sleep() {
      MQTT_topicSleep = read_Config("MQTT", "topicSleep");
    }
    void set_MQTT_Topic_Advertise_Unit() {
      MQTT_topicAdvertiseUnit = read_Config("MQTT", "topicAdvertiseUnit");
    }
    void set_MQTT_Topic_Unit() {
      MQTT_topicUnit = WiFi.macAddress();
    }
    void set_MQTT_Topic_Control() {
      MQTT_topicControl = read_Config("MQTT", "topicControl");
    }
    void set_MQTT_Topic_Call() {
      MQTT_topicCall = read_Config("MQTT", "topicCall");
    }
    void set_MQTT_Username() {
      MQTT_username = read_Config("MQTT", "username");
    }
    void set_MQTT_Password() {
      MQTT_password = read_Config("MQTT", "password");
    }
    void set_MQTT_Port() {
      MQTT_port = atoi(read_Config("MQTT", "port"));
    }

    // Song URL Configuration
    void set_Song_URL() {
      SONG_URL = read_Config("URL", "song");
    }

    //Settin all by one function
    void set_Network(){
      set_WiFi_SSID();
      set_WiFi_Password();
      set_MQTT_Broker();
      set_MQTT_Topic_Ringing();
      set_MQTT_Topic_Sleep();
      set_MQTT_Topic_Advertise_Unit();
      set_MQTT_Topic_Unit();
      set_MQTT_Topic_Control();
      set_MQTT_Username();
      set_MQTT_Password();
      set_MQTT_Port();
      set_Song_URL();
    }

    // I2S pins
    int8_t get_I2S_Dout(){
      return I2S_Dout;
    }
    int8_t get_I2S_Blck(){
      return I2S_Blck;
    }
    int8_t get_I2S_Lrc(){
      return I2S_Lrc;
    }

    // WiFi configuration
    String get_WiFi_SSID(){
      return WiFi_ssid;
    }
    String get_WiFi_Password(){
      return WiFi_password;
    }
    // MQTT Configuration
    String get_MQTT_Broker() {
      return MQTT_broker;
    }
    String get_MQTT_Topic_Ringing() {
      return MQTT_topicRinging;
    }
    String get_MQTT_Topic_Sleep() {
      return MQTT_topicSleep;
    }
    String get_MQTT_Topic_Advertise_Unit() {
      return MQTT_topicAdvertiseUnit;
    }
    String get_MQTT_Topic_Unit() {
      return MQTT_topicUnit;
    }
    String get_MQTT_Topic_Control() {
      return MQTT_topicControl;
    }
    String get_MQTT_Topic_Call() {
      return MQTT_topicCall;
    }
    String get_MQTT_Username() {
      return MQTT_username;
    }
    String get_MQTT_Password() {
      return MQTT_password;
    }
    inline String get_MQTT_client_id() {
      return MQTT_client_id + String(WiFi.macAddress());
    }
    int get_MQTT_Port() {
      return MQTT_port;
    }

    // Song URL Configuration
    String get_Song_URL() {
      return SONG_URL;
    }

    // Audio Configuration
    int get_Audio_Volume() {
      return AUDIO_volume;
    }

    // Serial Configuration
    int get_Serial_Baudrate() {
      return SERIAL_baudrate;
    }
};

// Create config object
Config config;
// Create audio object
Audio audio;
// Create WI-Fi client object and MQTT client object
WiFiClient espClient;
PubSubClient client(espClient);
bool call = false;

// SETUP // * ///////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(config.get_Serial_Baudrate());
  //Mount the SPIFFS  
  if (!SPIFFS.begin()){
    while (1) {
      Serial.println("SPIFFS.begin() failed");
    }
  }

  if (!ini.open()) {
    Serial.print("configuration file ");
    Serial.print(file);
    Serial.println(" does not exist");
    Serial.println("Fix it and restart the program");
    // Cannot do anything else
    while (1) {;}

  }
  Serial.println("configuration file exists");
  if (!ini.validate(buffer, bufferLen)) {
    Serial.print("configuration file ");
    Serial.print(ini.getFilename());
    Serial.print(" not valid: ");
    printErrorMessage(ini.getError());
    Serial.println("Fix it and restart the program");
    // Cannot do anything else
    while (1) {;}
  }

  config.set_Network();
  
  // Setup WiFi in Station mode
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.get_WiFi_SSID(), config.get_WiFi_Password());
  for (int i = 1; WiFi.status() != WL_CONNECTED; i++) {
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
  client.setServer(config.get_MQTT_Broker().c_str(), config.get_MQTT_Port());
  client.setCallback(callback);
  while (!client.connected()) {
    Serial.printf("The client %s connects to the public mqtt broker\n", config.get_MQTT_client_id().c_str());
    if (client.connect(config.get_MQTT_client_id().c_str(), config.get_MQTT_Username().c_str(), config.get_MQTT_Password().c_str())) {
      Serial.println("Private mqtt broker connected");
      Serial.println(client.state());
      mqtt_channals();
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }

  //Audio settings
  // Connect MAX98357 I2S Amplifier Module
  audio.setPinout(config.get_I2S_Blck(), config.get_I2S_Lrc(), config.get_I2S_Dout());
  // Set thevolume (0-100)
  audio.setVolume(config.get_Audio_Volume());
}

void loop() {
  client.loop();
  audio.loop();
  if (!WiFi.isConnected()) {
    reconect_WiFi();
  }
  if (!client.connected()) {
    reconect_MQTT();
  }
  if (audio.getAudioCurrentTime() <= 5 && !call){
    if (top < audio.getAudioCurrentTime()){
      top += 1;
      audio.setVolume(top*2);
    }
  }  
  if (audio.getAudioCurrentTime() >= 10 && !call){
    audio.setVolume(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 10));
    Serial.println(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 10)); // audio fade out
    //if(true){
    if(config.get_Audio_Volume() - (audio.getAudioCurrentTime() - 10) <= 1){
      audio.stopSong();
      audio.connecttohost("none");
      Serial.println("stopped");
      top = 0;
    }
  }
}

void mqtt_channals(){
  client.publish(config.get_MQTT_Topic_Advertise_Unit().c_str(), WiFi.macAddress().c_str());
  client.subscribe(config.get_MQTT_Topic_Ringing().c_str());
  client.subscribe(config.get_MQTT_Topic_Sleep().c_str());
  client.subscribe(config.get_MQTT_Topic_Control().c_str());
  client.subscribe(config.get_MQTT_Topic_Unit().c_str());
}

void reconect_WiFi(){
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.get_WiFi_SSID(), config.get_WiFi_Password());
  while (!WiFi.isConnected()) {
    Serial.print("Attempting MQTT connection...");
    if (WiFi.begin(config.get_WiFi_SSID(), config.get_WiFi_Password())) {
      Serial.println("WiFi reconnected");
    } else {
      Serial.print("failed, rc=");
      Serial.print(WiFi.status());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void reconect_MQTT() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(config.get_MQTT_client_id().c_str(), config.get_MQTT_Username().c_str(), config.get_MQTT_Password().c_str())) {
      Serial.println("MQTT reconnected");
      mqtt_channals();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
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
      client.publish(config.get_MQTT_Topic_Advertise_Unit().c_str(), WiFi.macAddress().c_str());
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

  else if (sTopic == config.get_MQTT_Topic_Call()){
    String CALL_ID = "";
    for (int i = 0; i < length; i++) {
      CALL_ID += (char) payload[i];
    }
    String SONG_path = config.get_Song_URL() + CALL_ID;
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
  else if (sTopic == "live"){
    String live = "";
    for (int i = 0; i < length; i++) {
      live += (char) payload[i];
    }
    audio.connecttohost(live.c_str());
    audio.setVolume(config.get_Audio_Volume());

    // Run audio player
    audio.loop();
    // kontrolný výpis
    if (!audio.isRunning()) {
      Serial.println("koniec zvonenia");
    }
  }

  else if (sTopic == WiFi.macAddress()){
    String controlPARAM = "";
    for (int i = 0; i < length; i++) {
      controlPARAM += (char) payload[i];
    }
    Serial.println(controlPARAM);
    if (controlPARAM == "diagnose"){
      diagnose(WiFi.macAddress());
    }
  }
}

inline char* read_Config(const char* section, const char* key){
  ini.getValue(section, key, buffer, bufferLen);
  return buffer;
}

String diagnose(String mac){
  // TODO diagnostiku vštkého okrem ESP32 a zdroja
  String diagnose_topic = "diagnose/" + WiFi.macAddress();
  client.publish(diagnose_topic.c_str(), stats.c_str());
  return "diagnose json";
}


void printErrorMessage(uint8_t e, bool eol)
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