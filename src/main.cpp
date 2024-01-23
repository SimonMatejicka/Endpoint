// Include required libraries
#include "Arduino.h"
#include "WiFi.h"
#include "Audio.h"
#include "PubSubClient.h"

// Structure Config with default configuration parameters
struct Config{
  // I2S          // TODO: on board pin names 
  int8_t I2S_Dout = 22; // DIN
  int8_t I2S_Blck = 26; // BCK
  int8_t I2S_Lrc = 25;  // LCK
  //Wi-Fi
  const char *WiFi_ssid = "Siet_rozhlas";
  const char *WiFi_password = "rozhlas123";
  //MQTT
  const char *MQTT_broker = "192.168.88.251";
  const char *MQTT_topic = "zvonenie";
  const char *MQTT_topicSleep = "sleep";
  const char *MQTT_username = "espClient";
  const char *MQTT_password = "client123";
  const int MQTT_port = 1884;
  // Path to song
  String SONG_URL = "http://192.168.88.252:80/zvonenie/";
  // Audio
  int AUDIO_volume = 25;
  // Serial monitor boudrate
  int SERIAL_boudrate = 115200;
};

// Create config object
Config config;
// Create audio object
Audio audio;
// Create WI-Fi client object and MQTT client object
WiFiClient espClient;
PubSubClient client(espClient);


void goToDeepSleep(long long sleep_time)
{
    Serial.println("Going to sleep...");
    // Configure the timer to wake us up! Time in uS
    esp_sleep_enable_timer_wakeup(sleep_time * 60LL * 1000000LL);
    // Go to sleep! Zzzz
    esp_deep_sleep_start();
}


void callback(const char *topic, byte *payload, unsigned int length) {
  Serial.println("in call back");
  String Stopic = topic;
  if (Stopic == "sleep") {
    String time_to_sleep = "";
    for (int i = 0; i < length; i++) {
      time_to_sleep += (char) payload[i];
    }
    Serial.println(time_to_sleep);
    goToDeepSleep((long long)time_to_sleep.toInt());
  }
  else if (Stopic == "zvonenie") {
    String SONG_ID = "";
    for (int i = 0; i < length; i++) {
      SONG_ID += (char) payload[i];
    }
    Serial.println(SONG_ID);
    String pathToSong = config.SONG_URL + SONG_ID;
    // Pripája sa
    int fails = 0;
    while (!audio.isRunning()) {
      delay(10);
      fails++;
      audio.connecttohost(pathToSong.c_str());
      if(fails == 10) {
        break;
      } 
    }
    audio.setVolume(config.AUDIO_volume);
    // Run audio player
 
    audio.loop();
    if (!audio.isRunning()) {
      Serial.println("koniec zvonenia");
    }
  }
  Serial.println("out call back");
}

void setup() {
  // Start Serial Monitor
  Serial.begin(config.SERIAL_boudrate);

  // Setup WiFi in Station mode
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.WiFi_ssid, config.WiFi_password);

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
  client.setServer(config.MQTT_broker, config.MQTT_port);
  client.setCallback(callback);
  while (!client.connected()) {
    String client_id = "esp32-client-";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public mqtt broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), config.MQTT_username, config.MQTT_password)) {
      Serial.println("Public emqx mqtt broker connected");
    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
    }
  }
  client.subscribe(config.MQTT_topic);
  client.subscribe(config.MQTT_topicSleep);
  Serial.println(config.MQTT_topic);

  //Audio settings
  // Connect MAX98357 I2S Amplifier Module
  audio.setPinout(config.I2S_Blck, config.I2S_Lrc, config.I2S_Dout);
  // Set thevolume (0-100)
  audio.setVolume(config.AUDIO_volume);
}



void loop() {
  client.loop();
  audio.loop();
  if (audio.getAudioCurrentTime() >= 20){
    audio.setVolume(config.AUDIO_volume - (audio.getAudioCurrentTime() - 20));
    Serial.println(config.AUDIO_volume - (audio.getAudioCurrentTime() - 20)); // audio fade out
    if(config.AUDIO_volume - (audio.getAudioCurrentTime() - 20) <= 1){
      audio.pauseResume();
      audio.connecttohost("none");
      Serial.println("stopped");
      // TODO kontrola času či je po 15:15

    }
  }
}

// Audio status functions
// ! treba preriediť !!!!
// TODO: vyčistiť

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