/*
 Receptor ESP32 para audio multicast.
*/
#include <Arduino.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TFT_eSPI.h>
#include <AudioFileSource.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include "wifi_credentials.h"

namespace {

const IPAddress AUDIO_MULTICAST_GROUP(239, 255, 42, 99);
const uint16_t AUDIO_UDP_PORT = 12345;
const size_t AUDIO_PACKET_HEADER_BYTES = 8;
const size_t AUDIO_PACKET_PAYLOAD_BYTES = 1200;
const size_t AUDIO_PACKET_BYTES =
    AUDIO_PACKET_HEADER_BYTES + AUDIO_PACKET_PAYLOAD_BYTES;
const uint32_t AUDIO_BUFFER_SIZE = 16384;
const uint32_t WIFI_RETRY_INTERVAL_MS = 10000;
const uint32_t AUDIO_READ_TIMEOUT_MS = 3000;
const uint32_t MULTICAST_RETRY_INTERVAL_MS = 5000;
const int16_t TOUCH_X_MIN = 200;
const int16_t TOUCH_X_MAX = 3800;
const int16_t TOUCH_Y_MIN = 200;
const int16_t TOUCH_Y_MAX = 3800;
const uint8_t VOLUME_STEP_PERCENT = 10;
const int16_t VOLUME_BUTTON_Y = 354;
const int16_t VOLUME_BUTTON_WIDTH = 64;
const int16_t VOLUME_BUTTON_HEIGHT = 56;
const int16_t VOLUME_DOWN_X = 32;
const int16_t VOLUME_UP_X = 224;
const uint32_t VOLUME_GAIN_RAMP_INTERVAL_MS = 4;
const uint32_t NETWORK_STATUS_UPDATE_INTERVAL_MS = 2000;

#define SPEAKER_ENABLE_PIN 4
#define XPT2046_IRQ 36
#define XPT2046_MOSI 13
#define XPT2046_MISO 12
#define XPT2046_CLK 14
#define XPT2046_CS 33

TFT_eSPI tft;
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);
WiFiUDP audioUdp;
AudioFileSourceBuffer* audioBuffer = nullptr;
AudioGeneratorMP3* audioDecoder = nullptr;
AudioOutputI2S* audioOutput = nullptr;
uint32_t lastWifiAttemptAt = 0;
uint32_t lastMulticastAttemptAt = 0;
bool audioStarted = false;
bool touchWasDown = false;
uint8_t speakerVolumePercent = 35;
float appliedSpeakerGain = speakerVolumePercent / 100.0f;
uint32_t lastVolumeGainUpdateAt = 0;
uint32_t lastNetworkStatusUpdateAt = 0;
wl_status_t lastDisplayedWifiStatus = WL_IDLE_STATUS;
bool networkStatusDrawn = false;

void drawVolumeValue() {
  const int16_t valueX = 112;
  const int16_t valueY = 360;
  const int16_t valueWidth = 96;
  const int16_t valueHeight = 32;
  const int16_t barX = 112;
  const int16_t barY = 398;
  const int16_t barWidth = 96;

  tft.fillRect(valueX, valueY, valueWidth, valueHeight, TFT_DARKGREY);
  char volumeLabel[8];
  snprintf(volumeLabel, sizeof(volumeLabel), "%u%%", speakerVolumePercent);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(volumeLabel, tft.width() / 2, 376);
  tft.fillRoundRect(barX, barY, barWidth, 10, 5, TFT_BLACK);
  const int16_t filledWidth = barWidth * speakerVolumePercent / 100;
  if (filledWidth > 0) {
    tft.fillRoundRect(barX, barY, filledWidth, 10, 5, TFT_GREEN);
  }
}

void drawVolumeControls() {
  const int16_t panelY = 328;

  tft.fillRoundRect(16, panelY, tft.width() - 32, 112, 10, TFT_DARKGREY);
  tft.drawRoundRect(16, panelY, tft.width() - 32, 112, 10, TFT_LIGHTGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString("VOLUMEN", tft.width() / 2, 343);

  tft.fillRoundRect(VOLUME_DOWN_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_NAVY);
  tft.drawRoundRect(VOLUME_DOWN_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_WHITE);
  tft.fillRoundRect(VOLUME_UP_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_NAVY);
  tft.drawRoundRect(VOLUME_UP_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_WHITE);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.drawString("-", VOLUME_DOWN_X + VOLUME_BUTTON_WIDTH / 2,
                 VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT / 2);
  tft.drawString("+", VOLUME_UP_X + VOLUME_BUTTON_WIDTH / 2,
                 VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT / 2);
  drawVolumeValue();
}

void updateAudioGain() {
  if (audioOutput == nullptr) {
    return;
  }

  const float targetGain = speakerVolumePercent / 100.0f;
  if (appliedSpeakerGain == targetGain) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastVolumeGainUpdateAt < VOLUME_GAIN_RAMP_INTERVAL_MS) {
    return;
  }
  lastVolumeGainUpdateAt = now;

  const float gainStep = 1.0f / 64.0f;
  if (appliedSpeakerGain < targetGain) {
    appliedSpeakerGain = min(appliedSpeakerGain + gainStep, targetGain);
  } else {
    appliedSpeakerGain = max(appliedSpeakerGain - gainStep, targetGain);
  }
  audioOutput->SetGain(appliedSpeakerGain);
}

void drawNetworkStatus(wl_status_t wifiStatus, int32_t rssi) {
  const bool connected = wifiStatus == WL_CONNECTED;
  uint16_t color = TFT_RED;
  if (connected) {
    color = rssi >= -60 ? TFT_GREEN : rssi >= -75 ? TFT_YELLOW : TFT_ORANGE;
  }

  tft.fillRoundRect(16, 124, tft.width() - 32, 40, 8, TFT_DARKGREY);
  tft.drawRoundRect(16, 124, tft.width() - 32, 40, 8, color);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_DARKGREY);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(connected ? "RED TX: CONECTADA" : "RED TX: DESCONECTADA",
                 28, 144);

  if (connected) {
    char rssiLabel[20];
    snprintf(rssiLabel, sizeof(rssiLabel), "RSSI: %ld dBm",
             static_cast<long>(rssi));
    tft.setTextDatum(MR_DATUM);
    tft.drawString(rssiLabel, tft.width() - 28, 144);
  }
}

void updateNetworkStatus(bool force = false) {
  const wl_status_t wifiStatus = WiFi.status();
  const uint32_t now = millis();
  if (!force && networkStatusDrawn && wifiStatus == lastDisplayedWifiStatus &&
      now - lastNetworkStatusUpdateAt < NETWORK_STATUS_UPDATE_INTERVAL_MS) {
    return;
  }

  const int32_t rssi = wifiStatus == WL_CONNECTED ? WiFi.RSSI() : 0;
  drawNetworkStatus(wifiStatus, rssi);
  lastNetworkStatusUpdateAt = now;
  lastDisplayedWifiStatus = wifiStatus;
  networkStatusDrawn = true;
}

void drawStatus(const char* title, const char* message, uint16_t color) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("RECEPTOR DE AUDIO", tft.width() / 2, 100);

  tft.fillRoundRect(16, 180, tft.width() - 32, 112, 10, TFT_DARKGREY);
  tft.drawRoundRect(16, 180, tft.width() - 32, 112, 10, color);
  tft.setTextFont(2);
  tft.setTextColor(color, TFT_DARKGREY);
  tft.drawString(title, tft.width() / 2, 218);
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(message, tft.width() / 2, 254);
  updateNetworkStatus(true);
  drawVolumeControls();
}

bool getTouchPosition(int16_t& x, int16_t& y) {
  const TS_Point point = touchscreen.getPoint();
  if (point.x < TOUCH_X_MIN || point.x > TOUCH_X_MAX ||
      point.y < TOUCH_Y_MIN || point.y > TOUCH_Y_MAX) {
    return false;
  }

  x = map(point.x, TOUCH_X_MIN, TOUCH_X_MAX, 0, tft.width() - 1);
  y = map(point.y, TOUCH_Y_MIN, TOUCH_Y_MAX, 0, tft.height() - 1);
  return true;
}

void updateVolumeTouch() {
  const bool isTouching = touchscreen.touched();
  if (isTouching && !touchWasDown) {
    int16_t touchX;
    int16_t touchY;
    bool volumeChanged = false;
    if (getTouchPosition(touchX, touchY) &&
        touchY >= VOLUME_BUTTON_Y &&
        touchY < VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT) {
      if (touchX >= VOLUME_DOWN_X &&
          touchX < VOLUME_DOWN_X + VOLUME_BUTTON_WIDTH) {
        speakerVolumePercent =
            speakerVolumePercent < VOLUME_STEP_PERCENT
                ? 0
                : speakerVolumePercent - VOLUME_STEP_PERCENT;
        volumeChanged = true;
      } else if (touchX >= VOLUME_UP_X &&
                 touchX < VOLUME_UP_X + VOLUME_BUTTON_WIDTH) {
        speakerVolumePercent =
            speakerVolumePercent > 100 - VOLUME_STEP_PERCENT
                ? 100
                : speakerVolumePercent + VOLUME_STEP_PERCENT;
        volumeChanged = true;
      }
      if (volumeChanged) {
        drawVolumeValue();
      }
    }
  }
  touchWasDown = isTouching;
}

class UdpAudioSource : public AudioFileSource {
 public:
  explicit UdpAudioSource(WiFiUDP& udp) : udp_(udp) {}

  bool begin() {
    open_ = udp_.beginMulticast(AUDIO_MULTICAST_GROUP, AUDIO_UDP_PORT) != 0;
    return open_;
  }

  uint32_t read(void* data, uint32_t len) override {
    uint8_t* destination = static_cast<uint8_t*>(data);
    uint32_t bytesRead = 0;
    const uint32_t startedAt = millis();

    while (bytesRead < len && millis() - startedAt < AUDIO_READ_TIMEOUT_MS) {
      bytesRead += readNonBlock(destination + bytesRead, len - bytesRead);
      if (bytesRead < len) {
        delay(1);
      }
    }
    return bytesRead;
  }

  uint32_t readNonBlock(void* data, uint32_t len) override {
    uint8_t* destination = static_cast<uint8_t*>(data);
    uint32_t bytesCopied = 0;

    while (bytesCopied < len) {
      if (packetOffset_ >= packetLength_ && !receivePacket()) {
        break;
      }

      const size_t available = packetLength_ - packetOffset_;
      const size_t toCopy =
          min(static_cast<size_t>(len - bytesCopied), available);
      memcpy(destination + bytesCopied, packet_ + packetOffset_, toCopy);
      packetOffset_ += toCopy;
      bytesCopied += toCopy;
      position_ += toCopy;
    }
    return bytesCopied;
  }

  bool hasPacket() {
    return packetOffset_ < packetLength_ || receivePacket();
  }

  bool seek(int32_t, int) override { return false; }
  bool close() override {
    udp_.stop();
    open_ = false;
    return true;
  }
  bool isOpen() override { return open_; }
  uint32_t getSize() override { return 0; }
  uint32_t getPos() override { return position_; }

 private:
  bool receivePacket() {
    while (true) {
      const int packetSize = udp_.parsePacket();
      if (packetSize <= 0) {
        return false;
      }
      if (packetSize < static_cast<int>(AUDIO_PACKET_HEADER_BYTES) ||
          packetSize > static_cast<int>(AUDIO_PACKET_BYTES)) {
        while (udp_.available()) {
          udp_.read();
        }
        continue;
      }

      const int received = udp_.read(packet_, packetSize);
      if (received != packetSize || packet_[0] != 'A' || packet_[1] != 'M' ||
          packet_[2] != 'R' || packet_[3] != '1') {
        continue;
      }

      const uint32_t sequence =
          (static_cast<uint32_t>(packet_[4]) << 24) |
          (static_cast<uint32_t>(packet_[5]) << 16) |
          (static_cast<uint32_t>(packet_[6]) << 8) |
          static_cast<uint32_t>(packet_[7]);
      if (hasSequence_ && sequence != expectedSequence_) {
        Serial.printf("Paquetes perdidos/reordenados: esperado %lu, recibido %lu\n",
                      static_cast<unsigned long>(expectedSequence_),
                      static_cast<unsigned long>(sequence));
      }
      expectedSequence_ = sequence + 1;
      hasSequence_ = true;

      packetLength_ = received - AUDIO_PACKET_HEADER_BYTES;
      packetOffset_ = 0;
      memmove(packet_, packet_ + AUDIO_PACKET_HEADER_BYTES, packetLength_);
      if (packetLength_ > 0) {
        return true;
      }
    }
  }

  WiFiUDP& udp_;
  uint8_t packet_[AUDIO_PACKET_BYTES];
  size_t packetLength_ = 0;
  size_t packetOffset_ = 0;
  uint32_t position_ = 0;
  uint32_t expectedSequence_ = 0;
  bool hasSequence_ = false;
  bool open_ = false;
};

UdpAudioSource* audioSource = nullptr;

void stopAudio() {
  digitalWrite(SPEAKER_ENABLE_PIN, HIGH);
  audioStarted = false;

  if (audioDecoder != nullptr) {
    if (audioDecoder->isRunning()) {
      audioDecoder->stop();
    }
    delete audioDecoder;
    audioDecoder = nullptr;
  }
  delete audioBuffer;
  audioBuffer = nullptr;
  if (audioSource != nullptr) {
    audioSource->close();
  }
  delete audioSource;
  audioSource = nullptr;
  if (audioOutput != nullptr) {
    audioOutput->stop();
    delete audioOutput;
    audioOutput = nullptr;
  }
}

bool startAudio() {
  audioBuffer = new AudioFileSourceBuffer(audioSource, AUDIO_BUFFER_SIZE);
  audioOutput = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
  audioOutput->SetOutputModeMono(true);
  appliedSpeakerGain = speakerVolumePercent / 100.0f;
  audioOutput->SetGain(appliedSpeakerGain);
  audioDecoder = new AudioGeneratorMP3();
  audioLogger = &Serial;
  if (!audioDecoder->begin(audioBuffer, audioOutput)) {
    Serial.println("No se pudo iniciar el decodificador MP3");
    stopAudio();
    drawStatus("ERROR DE AUDIO", "Esperando el flujo", TFT_RED);
    return false;
  }

  drawStatus("AUDIO DETECTADO", "Iniciando reproduccion", TFT_YELLOW);
  Serial.printf("Escuchando audio multicast en %s:%u\n",
                AUDIO_MULTICAST_GROUP.toString().c_str(), AUDIO_UDP_PORT);
  return true;
}

bool startAudioSource() {
  audioSource = new UdpAudioSource(audioUdp);
  if (!audioSource->begin()) {
    Serial.println("No se pudo unir al grupo multicast de audio");
    delete audioSource;
    audioSource = nullptr;
    drawStatus("ERROR MULTICAST", "Reintentando...", TFT_RED);
    return false;
  }
  Serial.println("Unido al grupo; esperando paquetes de audio...");
  drawStatus("CONECTADO", "Esperando audio...", TFT_CYAN);
  return true;
}

void updateWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    const uint32_t now = millis();
    if (audioSource == nullptr &&
        now - lastMulticastAttemptAt >= MULTICAST_RETRY_INTERVAL_MS) {
      lastMulticastAttemptAt = now;
      startAudioSource();
    }
    if (audioDecoder == nullptr && audioSource != nullptr &&
        audioSource->hasPacket()) {
      startAudio();
    }
    return;
  }

  if (audioDecoder != nullptr || audioSource != nullptr) {
    stopAudio();
    drawStatus("WIFI DESCONECTADO", "Reconectando...", TFT_YELLOW);
  }

  const uint32_t now = millis();
  if (now - lastWifiAttemptAt >= WIFI_RETRY_INTERVAL_MS) {
    lastWifiAttemptAt = now;
    Serial.println("Conectando a la red privada del transmisor...");
    WiFi.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif
  tft.init();
  tft.setRotation(0);
  Serial.printf("TFT inicializada: %d x %d\n", tft.width(), tft.height());
  drawStatus("INICIANDO", "Conectando al transmisor", TFT_YELLOW);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);

  pinMode(SPEAKER_ENABLE_PIN, OUTPUT);
  digitalWrite(SPEAKER_ENABLE_PIN, HIGH);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_AP_SSID, WIFI_AP_PASSWORD);
  lastWifiAttemptAt = millis();
  Serial.printf("Conectando a %s\n", WIFI_AP_SSID);
}

void loop() {
  updateVolumeTouch();
  updateWifi();
  updateAudioGain();
  updateNetworkStatus();

  if (audioDecoder != nullptr && audioDecoder->isRunning()) {
    if (!audioDecoder->loop()) {
      Serial.println("Se detuvo el decodificador; esperando el flujo multicast");
      stopAudio();
      drawStatus("FLUJO DETENIDO", "Esperando audio...", TFT_YELLOW);
    } else if (!audioStarted && audioSource != nullptr &&
               audioSource->getPos() > 0) {
      digitalWrite(SPEAKER_ENABLE_PIN, LOW);
      audioStarted = true;
      drawStatus("AUDIO RECIBIDO", "REPRODUCIENDO", TFT_GREEN);
      Serial.println("Reproduccion iniciada");
    }
  }

  delay(1);
}
