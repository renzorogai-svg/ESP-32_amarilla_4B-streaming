/*
18-10-2026 desde LapTop
reproduce striming.
***
El ESP32 emisor funciona en modo AP+STA: se conecta
al router de casa para recibir la radio por Internet y,
al mismo tiempo, crea una red Wi‑Fi privada para los receptores. 
El audio se envía por multicast a 239.255.42.99:12345 por la
interfaz de esa red privada, no por la del router.
**

  Driver: ST7796
  Type: TN
  Dots: 320x480
  Touch: RTP
  SKU  E32R40T

  http://tomeko.net/online_tools/file_to_hex.php?lang=en
*/
#include <Arduino.h>
#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <FS.h>
using fs::FS;
#include <WebServer.h>
#include <lwip/sockets.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <TJpg_Decoder.h>
#include "wifi_credentials.h"
#include "images/imagen_principal.h"

TFT_eSPI tft;
SPIClass touchscreenSPI = SPIClass(VSPI);

#define XPT2046_IRQ  36
#define XPT2046_MOSI 13
#define XPT2046_MISO 12
#define XPT2046_CLK  14
#define XPT2046_CS   33

#define SPEAKER_PIN 26
#define SPEAKER_ENABLE_PIN 4

XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

const int16_t TOUCH_X_MIN = 200;
const int16_t TOUCH_X_MAX = 3800;
const int16_t TOUCH_Y_MIN = 200;
const int16_t TOUCH_Y_MAX = 3800;

struct Station {
  const char* label;
  const char* url;
};

Station stations[] = {
  {"181 Eagle", "http://listen.181fm.com/181-eagle_128k.mp3"},
  {"ABBA", "http://streaming.exclusive.radio/er/abba/icecast.audio"},
  {"AC/DC", "http://streaming.exclusive.radio/er/acdc/icecast.audio"},
  {"Africa N.1", "http://webradio6.ice.infomaniak.ch/webradio6-128.mp3"},
  {"Allzic Brazil", "http://allzic19.ice.infomaniak.ch/allzic19.mp3"},
  {"Ambient Radio", "http://ambient.stream.laut.fm/ambient"},
  {"Bob Marley", "http://streaming.exclusive.radio/er/bobmarley/icecast.audio"},
  {"Chocolate FM", "http://streaming5.elitecomunicacion.es:8082/live.mp3"},
  {"Cyndi Lauper", "http://streaming.exclusive.radio/er/cyndilauper/icecast.audio"},
  {"David Bowie", "http://streaming.exclusive.radio/er/davidbowie/icecast.audio"},
  {"Depeche Mode", "http://streaming.exclusive.radio/er/depechemode/icecast.audio"},
  {"Devo", "http://streaming.exclusive.radio/er/devo/icecast.audio"},
  {"Dire Straits", "http://streaming.exclusive.radio/er/direstraits/icecast.audio"},
  {"Eagles", "http://streaming.exclusive.radio/er/eagles/icecast.audio"},
  {"Elton John", "http://streaming.exclusive.radio/er/eltonjohn/icecast.audio"},
  {"Eurythmics", "http://streaming.exclusive.radio/er/eurythmics/icecast.audio"},
  {"Fleetwood Mac", "http://streaming.exclusive.radio/er/fleetwoodmac/icecast.audio"},
  {"FUN Radio", "http://icecast.funradio.fr/fun-1-44-128"},
  {"Genesis", "http://streaming.exclusive.radio/er/genesis/icecast.audio"},
  {"Genesis", "http://streaming.exclusive.radio/er/genesis/icecast.audio"},
  {"Grosse Radio Reggae", "http://hd.lagrosseradio.info/lagrosseradio-reggae-192.mp3"},
  {"HitMusic 80s", "http://sc4.easywebcommunications.com:18004/stream/1/"},
  {"Italian Vintage", "http://maggie.torontocast.com:8018/stream/1/"},
  {"KVKVI Hits", "http://dc1.serverse.com:7294/stream/1/"},
  {"Led Zeppelin", "http://streaming.exclusive.radio/er/ledzeppelin/icecast.audio"},
  {"Madonna", "http://streaming.exclusive.radio/er/madonna/icecast.audio"},
  {"Madness", "http://streaming.exclusive.radio/er/madness/icecast.audio"},
  {"Melodia Arequipa", "http://stream.radiomelodia.com.pe:8000/radio.mp3"},
  {"Metallica", "http://streaming.exclusive.radio/er/metallica/icecast.audio"},
  {"Michael Jackson", "http://streaming.exclusive.radio/er/michaeljackson/icecast.audio"},
  {"Pink", "http://streaming.exclusive.radio/er/pink/icecast.audio"},
  {"Pink Floyd", "http://streaming.exclusive.radio/er/pinkfloyd/icecast.audio"},
  {"Prince", "http://streaming.exclusive.radio/er/prince/icecast.audio"},
  {"ProgRock.com", "http://pavo.prostreaming.net:8012/stream"},
  {"QUEEN", "http://streaming.exclusive.radio/er/queen/icecast.audio"},
  {"Radio Bachata", "http://perseus.shoutca.st:8312/;stream/1"},
  {"Radio Maya Salsa", "http://s1.cmesolution.my:5266/"},
  {"Rastapuls Reggae", "http://radio.rastapuls.com:8000/rastapuls"},
  {"Rush", "http://streaming.exclusive.radio/er/rush/icecast.audio"},
  {"Sade", "http://streaming.exclusive.radio/er/sade/icecast.audio"},
  {"Salsa One Radio", "http://server7.servistreaming.com:10006/stream"},
  {"Supertramp", "http://streaming.exclusive.radio/er/supertramp/icecast.audio"},
  {"Supertramp", "http://streaming.exclusive.radio/er/supertramp/icecast.audio"},
  {"Swiss Jazz", "http://stream.srg-ssr.ch/srgssr/rsj/mp3/128"},
  {"Swiss Pop", "http://stream.srg-ssr.ch/srgssr/rsp/mp3/128"},
  {"Tears for Fears", "http://streaming.exclusive.radio/er/tearsforfears/icecast.audio"},
  {"The Beatles", "http://streaming.exclusive.radio/er/thebeatles/icecast.audio"},
  {"The Cure", "http://streaming.exclusive.radio/er/thecure/icecast.audio"},
  {"The Rolling Stones", "http://streaming.exclusive.radio/er/therollingstones/icecast.audio"},
  {"The Who", "http://streaming.exclusive.radio/er/thewho/icecast.audio"},
  {"Tina Turner", "http://streaming.exclusive.radio/er/tinaturner/icecast.audio"},
  {"Toto", "http://streaming.exclusive.radio/er/toto/icecast.audio"},
  {"Tropicanal", "http://stream.zeno.fm/kmnekh2929duv"},
  {"U2", "http://streaming.exclusive.radio/er/u2/icecast.audio"},
  {"Wham!", "http://streaming.exclusive.radio/er/wham/icecast.audio"},
  {"Whitney Houston", "http://streaming.exclusive.radio/er/whitneyhouston/icecast.audio"},
  {"Yes", "http://streaming.exclusive.radio/er/yes/icecast.audio"},
   {"Eurodance", "http://rn2.fluidstream.eu/gammaradio.mp3"},

};

const size_t STATION_COUNT = sizeof(stations) / sizeof(stations[0]);
const uint32_t RADIO_BUFFER_SIZE = 16384;
const uint32_t RADIO_START_TIMEOUT_MS = 10000;
const uint16_t STATION_COLORS[] = {
    TFT_NAVY,   TFT_MAROON,    TFT_PURPLE,  TFT_OLIVE, TFT_BLUE,
    TFT_BROWN,  TFT_DARKCYAN,  TFT_DARKGREEN, TFT_DARKGREY, 0x4208};
const uint8_t VOLUME_STEP_PERCENT = 10;
const size_t STATION_COLUMN_COUNT = 1;
const int16_t STATION_LIST_X = 8;
const int16_t STATION_LIST_Y = 48;
const int16_t STATION_LIST_HEIGHT = 226;
const int16_t STATION_BUTTON_WIDTH = 146;
const int16_t STATION_BUTTON_HEIGHT = 32;
const int16_t STATION_BUTTON_SPACING = 36;
const int16_t STATION_NUMBER_AREA_WIDTH = 20;
const int16_t STATION_COLUMN_GAP = 12;
const size_t STATION_ROW_COUNT =
    (STATION_COUNT + STATION_COLUMN_COUNT - 1) / STATION_COLUMN_COUNT;
const size_t VISIBLE_STATION_ROWS =
    (STATION_LIST_HEIGHT - STATION_BUTTON_HEIGHT) / STATION_BUTTON_SPACING +
    1;
const int16_t SCROLLBAR_X = 316;
const int16_t SCROLLBAR_WIDTH = 4;
const int16_t PLAY_BUTTON_X = 8;
const int16_t PAUSE_BUTTON_X = 84;
const int16_t TRANSPORT_BUTTON_Y = 346;
const int16_t TRANSPORT_BUTTON_WIDTH = 68;
const int16_t TRANSPORT_BUTTON_HEIGHT = 44;
const int16_t VOLUME_BUTTON_Y = TRANSPORT_BUTTON_Y;
const int16_t VOLUME_BUTTON_HEIGHT = TRANSPORT_BUTTON_HEIGHT;
const int16_t VOLUME_DOWN_X = 160;
const int16_t VOLUME_UP_X = 236;
const int16_t VOLUME_BUTTON_WIDTH = 68;
const int16_t VOLUME_LABEL_X = 108;
const int16_t VOLUME_LABEL_Y = 394;
const uint16_t VOLUME_LABEL_WIDTH = 104;
const uint16_t VOLUME_LABEL_HEIGHT = 28;
const uint16_t BACKGROUND_IMAGE_WIDTH = 320;
const uint16_t BACKGROUND_IMAGE_HEIGHT = 480;
const IPAddress AUDIO_AP_IP(192, 168, 50, 1);
const IPAddress AUDIO_AP_SUBNET(255, 255, 255, 0);
WebServer controlServer(80);
const IPAddress AUDIO_MULTICAST_GROUP(239, 255, 42, 99);
const uint16_t AUDIO_UDP_PORT = 12345;
const size_t AUDIO_PACKET_PAYLOAD_BYTES = 1200;
const size_t AUDIO_PACKET_HEADER_BYTES = 8;
uint8_t speakerVolumePercent = 35;
bool touchWasDown = false;
bool touchStartedInStationList = false;
bool touchMoved = false;
int16_t touchStartX = 0;
int16_t touchStartY = 0;
int16_t touchLastY = 0;
int16_t scrollDistance = 0;
size_t stationScrollOffset = 0;
bool stationListNeedsRedraw = false;
size_t stationRedrawIndex = 0;
bool radioRequested = false;
bool radioPaused = false;
bool audioNeedsUnmute = false;
int8_t activeStationIndex = -1;
size_t stationAttempts = 0;
uint32_t radioAttemptStartedAt = 0;
int audioMulticastSocket = -1;
bool audioMulticastReady = false;

bool beginAudioMulticast() {
  audioMulticastSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (audioMulticastSocket < 0) {
    Serial.println("No se pudo crear el socket UDP de audio");
    return false;
  }

  in_addr multicastInterface = {};
  multicastInterface.s_addr = static_cast<uint32_t>(AUDIO_AP_IP);
  const uint8_t multicastTtl = 1;
  if (setsockopt(audioMulticastSocket, IPPROTO_IP, IP_MULTICAST_IF,
                 &multicastInterface, sizeof(multicastInterface)) < 0 ||
      setsockopt(audioMulticastSocket, IPPROTO_IP, IP_MULTICAST_TTL,
                 &multicastTtl, sizeof(multicastTtl)) < 0) {
    Serial.println("No se pudo configurar multicast en el AP privado");
    closesocket(audioMulticastSocket);
    audioMulticastSocket = -1;
    return false;
  }
  return true;
}

class MulticastAudioSource : public AudioFileSource {
 public:
  explicit MulticastAudioSource(AudioFileSource* source) : source_(source) {}

  uint32_t read(void* data, uint32_t len) override {
    const uint32_t bytesRead = source_->read(data, len);
    sendAudio(data, bytesRead);
    return bytesRead;
  }

  uint32_t readNonBlock(void* data, uint32_t len) override {
    const uint32_t bytesRead = source_->readNonBlock(data, len);
    sendAudio(data, bytesRead);
    return bytesRead;
  }

  bool open(const char* filename) override { return source_->open(filename); }
  bool seek(int32_t pos, int dir) override { return source_->seek(pos, dir); }
  bool close() override { return source_->close(); }
  bool isOpen() override { return source_->isOpen(); }
  uint32_t getSize() override { return source_->getSize(); }
  uint32_t getPos() override { return source_->getPos(); }
  bool loop() override { return source_->loop(); }

 private:
  void sendAudio(const void* data, uint32_t length) {
    if (!audioMulticastReady || WiFi.status() != WL_CONNECTED || length == 0) {
      return;
    }

    const uint8_t* audio = static_cast<const uint8_t*>(data);
    sockaddr_in destination = {};
    destination.sin_family = AF_INET;
    destination.sin_port = htons(AUDIO_UDP_PORT);
    destination.sin_addr.s_addr =
        static_cast<uint32_t>(AUDIO_MULTICAST_GROUP);
    size_t offset = 0;
    while (offset < length) {
      const size_t audioBytes =
          min(static_cast<size_t>(length) - offset,
              AUDIO_PACKET_PAYLOAD_BYTES);
      uint8_t packet[AUDIO_PACKET_HEADER_BYTES + AUDIO_PACKET_PAYLOAD_BYTES] = {
          'A',
          'M',
          'R',
          '1',
          static_cast<uint8_t>(sequence_ >> 24),
          static_cast<uint8_t>(sequence_ >> 16),
          static_cast<uint8_t>(sequence_ >> 8),
          static_cast<uint8_t>(sequence_)};
      ++sequence_;
      memcpy(packet + AUDIO_PACKET_HEADER_BYTES, audio + offset, audioBytes);
      const size_t packetLength = AUDIO_PACKET_HEADER_BYTES + audioBytes;
      const ssize_t sentBytes =
          sendto(audioMulticastSocket, packet, packetLength, 0,
                 reinterpret_cast<sockaddr*>(&destination),
                 sizeof(destination));
      if (sentBytes != static_cast<ssize_t>(packetLength)) {
        const uint32_t now = millis();
        if (now - lastErrorLogAt_ >= 5000) {
          Serial.println("Error enviando audio por multicast UDP");
          lastErrorLogAt_ = now;
        }
      }
      offset += audioBytes;
    }
  }

  AudioFileSource* source_;
  uint32_t sequence_ = 0;
  uint32_t lastErrorLogAt_ = 0;
};

AudioFileSourceICYStream* radioSource = nullptr;
MulticastAudioSource* multicastSource = nullptr;
AudioFileSourceBuffer* radioBuffer = nullptr;
AudioGeneratorMP3* radioDecoder = nullptr;
AudioOutputI2S* radioOutput = nullptr;
uint16_t volumeLabelBackground[VOLUME_LABEL_WIDTH * VOLUME_LABEL_HEIGHT];
bool volumeLabelBackgroundReady = false;

bool drawBackgroundTile(int16_t x, int16_t y, uint16_t width,
                        uint16_t height, uint16_t* pixels) {
  if (width > 16 || height > 16) {
    Serial.println("Bloque JPEG demasiado grande para el fondo");
    return false;
  }

  const int16_t overlapLeft = x > VOLUME_LABEL_X ? x : VOLUME_LABEL_X;
  const int16_t overlapTop = y > VOLUME_LABEL_Y ? y : VOLUME_LABEL_Y;
  const int16_t overlapRight =
      x + width < VOLUME_LABEL_X + VOLUME_LABEL_WIDTH
          ? x + width
          : VOLUME_LABEL_X + VOLUME_LABEL_WIDTH;
  const int16_t overlapBottom =
      y + height < VOLUME_LABEL_Y + VOLUME_LABEL_HEIGHT
          ? y + height
          : VOLUME_LABEL_Y + VOLUME_LABEL_HEIGHT;
  if (overlapLeft < overlapRight && overlapTop < overlapBottom) {
    for (int16_t screenY = overlapTop; screenY < overlapBottom; ++screenY) {
      for (int16_t screenX = overlapLeft; screenX < overlapRight; ++screenX) {
        const uint16_t tileColumn = screenX - x;
        const uint16_t tileRow = screenY - y;
        const size_t backgroundIndex =
            (screenY - VOLUME_LABEL_Y) * VOLUME_LABEL_WIDTH +
            (screenX - VOLUME_LABEL_X);
        volumeLabelBackground[backgroundIndex] =
            pixels[tileRow * width + tileColumn];
      }
    }
    volumeLabelBackgroundReady = true;
  }

  tft.pushImage(x, y, width, height, pixels);
  return true;
}

void drawBackgroundImage() {
  uint16_t imageWidth = 0;
  uint16_t imageHeight = 0;
  const JRESULT sizeResult =
      TJpgDec.getJpgSize(&imageWidth, &imageHeight, imagen, sizeof(imagen));
  if (sizeResult != JDR_OK || imageWidth != BACKGROUND_IMAGE_WIDTH ||
      imageHeight != BACKGROUND_IMAGE_HEIGHT) {
    Serial.printf(
        "Dimensiones de fondo incorrectas: %u x %u (se esperan %u x %u)\n",
        imageWidth, imageHeight, BACKGROUND_IMAGE_WIDTH,
        BACKGROUND_IMAGE_HEIGHT);
    volumeLabelBackgroundReady = false;
    return;
  }

  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(drawBackgroundTile);
  const JRESULT result =
      TJpgDec.drawJpg(0, 0, imagen, sizeof(imagen));
  if (result != JDR_OK) {
    Serial.printf("No se pudo dibujar la imagen de fondo (error %d)\n",
                  result);
    volumeLabelBackgroundReady = false;
  }
}

void muteSpeaker() {
  digitalWrite(SPEAKER_ENABLE_PIN, HIGH);
  audioNeedsUnmute = false;
}

void unmuteSpeakerAfterAudioStarts() {
  if (audioNeedsUnmute) {
    digitalWrite(SPEAKER_ENABLE_PIN, LOW);
    audioNeedsUnmute = false;
  }
}

void drawVolumeControls() {
  char volumeLabel[16];
  snprintf(volumeLabel, sizeof(volumeLabel), "%u%%", speakerVolumePercent);

  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.drawRoundRect(VOLUME_DOWN_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_WHITE);
  tft.drawRoundRect(VOLUME_UP_X, VOLUME_BUTTON_Y, VOLUME_BUTTON_WIDTH,
                    VOLUME_BUTTON_HEIGHT, 6, TFT_WHITE);
  tft.drawString("-", VOLUME_DOWN_X + VOLUME_BUTTON_WIDTH / 2,
                 VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT / 2);
  tft.drawString("+", VOLUME_UP_X + VOLUME_BUTTON_WIDTH / 2,
                 VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT / 2);
  if (volumeLabelBackgroundReady) {
    tft.pushImage(VOLUME_LABEL_X, VOLUME_LABEL_Y, VOLUME_LABEL_WIDTH,
                  VOLUME_LABEL_HEIGHT, volumeLabelBackground);
  }
  tft.drawString(volumeLabel, tft.width() / 2,
                 VOLUME_LABEL_Y + VOLUME_LABEL_HEIGHT / 2);
}

void drawTransportControls() {
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.drawRoundRect(PLAY_BUTTON_X, TRANSPORT_BUTTON_Y,
                    TRANSPORT_BUTTON_WIDTH, TRANSPORT_BUTTON_HEIGHT, 6,
                    TFT_WHITE);
  tft.drawRoundRect(PAUSE_BUTTON_X, TRANSPORT_BUTTON_Y,
                    TRANSPORT_BUTTON_WIDTH, TRANSPORT_BUTTON_HEIGHT, 6,
                    radioPaused ? TFT_GREEN : TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("PLAY", PLAY_BUTTON_X + TRANSPORT_BUTTON_WIDTH / 2,
                 TRANSPORT_BUTTON_Y + TRANSPORT_BUTTON_HEIGHT / 2);
  tft.drawString("PAUSA", PAUSE_BUTTON_X + TRANSPORT_BUTTON_WIDTH / 2,
                 TRANSPORT_BUTTON_Y + TRANSPORT_BUTTON_HEIGHT / 2);
}

void drawStatus(const char* message) {
  tft.setTextFont(2);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE);
  tft.fillRect(0, 438, tft.width(), 24, TFT_BLACK);
  tft.drawString(message, tft.width() / 2, 450);
}

int16_t stationButtonX(size_t column) {
  return STATION_LIST_X + STATION_NUMBER_AREA_WIDTH +
         column * (STATION_BUTTON_WIDTH + STATION_NUMBER_AREA_WIDTH +
                   STATION_COLUMN_GAP);
}

void drawStationButton(size_t stationIndex, size_t visibleRow, size_t column) {
  const int16_t x = stationButtonX(column);
  const int16_t y = STATION_LIST_Y + visibleRow * STATION_BUTTON_SPACING;
  tft.fillRect(x - STATION_NUMBER_AREA_WIDTH, y,
               STATION_BUTTON_WIDTH + STATION_NUMBER_AREA_WIDTH,
               STATION_BUTTON_SPACING, TFT_BLACK);
  if (stationIndex >= STATION_COUNT) {
    return;
  }

  const bool isPlaying = radioRequested && activeStationIndex == stationIndex;
  const uint16_t fillColor =
      STATION_COLORS[stationIndex % (sizeof(STATION_COLORS) /
                                     sizeof(STATION_COLORS[0]))];
  tft.fillRoundRect(x, y, STATION_BUTTON_WIDTH, STATION_BUTTON_HEIGHT, 5,
                    fillColor);
  tft.drawRoundRect(x, y, STATION_BUTTON_WIDTH, STATION_BUTTON_HEIGHT, 5,
                    isPlaying ? TFT_GREEN : TFT_LIGHTGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, fillColor);

  char stationLabel[16];
  snprintf(stationLabel, sizeof(stationLabel), "%.12s",
           stations[stationIndex].label);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(stationLabel, x + 6, y + STATION_BUTTON_HEIGHT / 2);

  char stationNumber[5];
  snprintf(stationNumber, sizeof(stationNumber), "%u",
           static_cast<unsigned int>(stationIndex + 1));
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(stationNumber,
                 x - STATION_NUMBER_AREA_WIDTH / 2,
                 y + STATION_BUTTON_HEIGHT / 2);

  if (isPlaying) {
    const int16_t centerY = y + STATION_BUTTON_HEIGHT / 2;
    tft.fillRect(x + STATION_BUTTON_WIDTH - 18, centerY - 3, 8, 6,
                 TFT_GREEN);
    tft.fillTriangle(x + STATION_BUTTON_WIDTH - 22, centerY,
                     x + STATION_BUTTON_WIDTH - 8, centerY - 10,
                     x + STATION_BUTTON_WIDTH - 8, centerY + 10,
                     TFT_GREEN);
    tft.drawTriangle(x + STATION_BUTTON_WIDTH - 22, centerY,
                     x + STATION_BUTTON_WIDTH - 8, centerY - 10,
                     x + STATION_BUTTON_WIDTH - 8, centerY + 10, TFT_WHITE);
  }
}

void drawStationScrollbar() {
  if (STATION_ROW_COUNT > VISIBLE_STATION_ROWS) {
    const int16_t thumbHeight =
        STATION_LIST_HEIGHT * VISIBLE_STATION_ROWS / STATION_ROW_COUNT;
    const size_t maxOffset = STATION_ROW_COUNT - VISIBLE_STATION_ROWS;
    const int16_t thumbY =
        STATION_LIST_Y +
        (STATION_LIST_HEIGHT - thumbHeight) * stationScrollOffset /
            maxOffset;
    tft.fillRect(SCROLLBAR_X, STATION_LIST_Y, SCROLLBAR_WIDTH,
                 STATION_LIST_HEIGHT, TFT_DARKGREY);
    tft.fillRoundRect(SCROLLBAR_X, thumbY, SCROLLBAR_WIDTH, thumbHeight, 2,
                      TFT_WHITE);
  }
}

void drawStationList() {
  const size_t visibleRows =
      STATION_ROW_COUNT < VISIBLE_STATION_ROWS
          ? STATION_ROW_COUNT
          : VISIBLE_STATION_ROWS;
  for (size_t visibleRow = 0; visibleRow < visibleRows; ++visibleRow) {
    for (size_t column = 0; column < STATION_COLUMN_COUNT; ++column) {
      const size_t stationIndex =
          (stationScrollOffset + visibleRow) * STATION_COLUMN_COUNT + column;
      drawStationButton(stationIndex, visibleRow, column);
    }
  }
  drawStationScrollbar();
  stationListNeedsRedraw = false;
}

void drawStationListStep() {
  const size_t visibleRows =
      STATION_ROW_COUNT < VISIBLE_STATION_ROWS
          ? STATION_ROW_COUNT
          : VISIBLE_STATION_ROWS;
  const size_t visibleButtonCount = visibleRows * STATION_COLUMN_COUNT;
  if (stationRedrawIndex < visibleButtonCount) {
    const size_t visibleRow = stationRedrawIndex / STATION_COLUMN_COUNT;
    const size_t column = stationRedrawIndex % STATION_COLUMN_COUNT;
    const size_t stationIndex =
        (stationScrollOffset + visibleRow) * STATION_COLUMN_COUNT + column;
    drawStationButton(stationIndex, visibleRow, column);
    ++stationRedrawIndex;
    return;
  }

  drawStationScrollbar();
  stationListNeedsRedraw = false;
}

void setStationScrollOffset(size_t offset) {
  const size_t maxOffset =
      STATION_ROW_COUNT > VISIBLE_STATION_ROWS
          ? STATION_ROW_COUNT - VISIBLE_STATION_ROWS
          : 0;
  if (offset > maxOffset) {
    offset = maxOffset;
  }
  if (stationScrollOffset != offset) {
    stationScrollOffset = offset;
    stationListNeedsRedraw = true;
    stationRedrawIndex = 0;
  }
}

void drawInterface() {
  drawBackgroundImage();
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);

  tft.setTextColor(TFT_WHITE);
  tft.drawString("Modo streaming", tft.width() / 2, 22);

  drawStationList();
  drawTransportControls();
  drawVolumeControls();
}

void playRadio();
void pauseRadio();

void adjustSpeakerVolume(bool increase) {
  const uint8_t updatedVolume =
      increase
          ? (speakerVolumePercent > 100 - VOLUME_STEP_PERCENT
                 ? 100
                 : speakerVolumePercent + VOLUME_STEP_PERCENT)
          : (speakerVolumePercent < VOLUME_STEP_PERCENT
                 ? 0
                 : speakerVolumePercent - VOLUME_STEP_PERCENT);
  if (updatedVolume == speakerVolumePercent) {
    return;
  }

  speakerVolumePercent = updatedVolume;
  drawVolumeControls();
  if (radioOutput != nullptr) {
    radioOutput->SetGain(speakerVolumePercent / 100.0f);
  }
}

void sendControlState() {
  char response[160];
  const char* station =
      activeStationIndex >= 0 ? stations[activeStationIndex].label : "";
  snprintf(response, sizeof(response),
           "{\"playing\":%s,\"paused\":%s,\"volume\":%u,\"station\":\"%s\"}",
           radioRequested && !radioPaused ? "true" : "false",
           radioPaused ? "true" : "false", speakerVolumePercent, station);
  controlServer.sendHeader("Cache-Control", "no-store");
  controlServer.send(200, "application/json; charset=utf-8", response);
}

const char CONTROL_PAGE[] PROGMEM = R"rawliteral(
<!doctype html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Control de radio</title>
  <style>
    body{font:16px system-ui,sans-serif;max-width:440px;margin:2rem auto;padding:0 1rem;background:#101820;color:#f4f7fa}
    h1{font-size:1.6rem}.panel{padding:1rem;border-radius:14px;background:#1c2a36}
    .controls{display:grid;grid-template-columns:1fr 1fr;gap:.75rem;margin-top:1rem}
    .controls form{margin:0}
    button{min-height:64px;border:0;border-radius:10px;font-size:1.1rem;font-weight:700;background:#20a44a;color:#ffffff}
    .button:active{transform:scale(.98)}#volume{font-size:1.4rem;font-weight:700}
    #message{min-height:1.5em;color:#ffc85a}
  </style>
</head>
<body>
  <h1>Control de radio</h1>
  <section class="panel">
    <div id="state">Conectando...</div>
    <p>Volumen: <span id="volume">--%</span></p>
    <div class="controls">
      <form action="/api/play" method="post"><button class="button" type="submit">PLAY</button></form>
      <form action="/api/pause" method="post"><button class="button" type="submit">PAUSA</button></form>
      <form action="/api/volume/down" method="post"><button class="button" type="submit">VOL -</button></form>
      <form action="/api/volume/up" method="post"><button class="button" type="submit">VOL +</button></form>
    </div>
  </section>
  <script>
    const stateElement=document.getElementById('state');
    const volumeElement=document.getElementById('volume');
    async function refresh(){
      try{
        const response=await fetch('/api/state',{cache:'no-store'});
        if(!response.ok)throw new Error('No se pudo consultar el estado');
        const data=await response.json();
        stateElement.textContent=data.station
          ? (data.paused?'En pausa: ':'')+(data.playing?'Reproduciendo: ':'')+data.station
          : 'Selecciona una emisora en la pantalla del transmisor';
        volumeElement.textContent=data.volume+'%';
      }catch(error){stateElement.textContent='Sin conexión con el transmisor'}
    }
    refresh();
    setInterval(refresh,2000);
  </script>
</body>
</html>
)rawliteral";

void startControlServer() {
  controlServer.on("/", HTTP_GET, []() {
    controlServer.sendHeader("Cache-Control", "no-store");
    controlServer.send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
  });
  controlServer.on("/api/state", HTTP_GET, sendControlState);
  controlServer.on("/api/play", HTTP_GET, []() {
    playRadio();
    sendControlState();
  });
  controlServer.on("/api/play", HTTP_POST, []() {
    playRadio();
    controlServer.sendHeader("Location", "/", true);
    controlServer.send(303, "text/plain", "");
  });
  controlServer.on("/api/pause", HTTP_GET, []() {
    pauseRadio();
    sendControlState();
  });
  controlServer.on("/api/pause", HTTP_POST, []() {
    pauseRadio();
    controlServer.sendHeader("Location", "/", true);
    controlServer.send(303, "text/plain", "");
  });
  controlServer.on("/api/volume/down", HTTP_GET, []() {
    adjustSpeakerVolume(false);
    sendControlState();
  });
  controlServer.on("/api/volume/down", HTTP_POST, []() {
    adjustSpeakerVolume(false);
    controlServer.sendHeader("Location", "/", true);
    controlServer.send(303, "text/plain", "");
  });
  controlServer.on("/api/volume/up", HTTP_GET, []() {
    adjustSpeakerVolume(true);
    sendControlState();
  });
  controlServer.on("/api/volume/up", HTTP_POST, []() {
    adjustSpeakerVolume(true);
    controlServer.sendHeader("Location", "/", true);
    controlServer.send(303, "text/plain", "");
  });
  controlServer.onNotFound([]() {
    controlServer.send(404, "text/plain; charset=utf-8", "No encontrado");
  });
  controlServer.begin();
  Serial.printf("Pagina de control: http://%s/\n",
                WiFi.softAPIP().toString().c_str());
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

void releaseRadioOutput() {
  muteSpeaker();
  if (radioDecoder != nullptr) {
    if (radioDecoder->isRunning()) {
      radioDecoder->stop();
    }
    delete radioDecoder;
    radioDecoder = nullptr;
  }
  delete radioBuffer;
  radioBuffer = nullptr;
  delete multicastSource;
  multicastSource = nullptr;
  delete radioSource;
  radioSource = nullptr;

  if (radioOutput != nullptr) {
    radioOutput->stop();
    delete radioOutput;
    radioOutput = nullptr;
  }

}

void stopRadio() {
  radioRequested = false;
  radioPaused = false;
  stationAttempts = 0;
  releaseRadioOutput();
  drawInterface();
  drawStatus("Radio detenida");
}

void pauseRadio() {
  if (!radioRequested || radioPaused) {
    return;
  }
  radioPaused = true;
  releaseRadioOutput();
  drawTransportControls();
  drawStationList();
  drawStatus("Radio en pausa");
}

void playRadio() {
  if (activeStationIndex < 0) {
    drawStatus("Selecciona una emisora");
    return;
  }
  if (radioRequested && !radioPaused) {
    return;
  }

  releaseRadioOutput();
  radioRequested = true;
  radioPaused = false;
  stationAttempts = 1;
  radioAttemptStartedAt = millis();
  drawTransportControls();
  drawStationList();

  if (WiFi.status() != WL_CONNECTED) {
    drawStatus("Conectando WiFi...");
    return;
  }
  drawStatus("Reanudando emisora...");
}

void failoverToNextStation(const char* reason) {
  Serial.printf("Fallo emisora %s: %s\n",
                stations[activeStationIndex].label, reason);
  releaseRadioOutput();

  if (stationAttempts >= STATION_COUNT) {
    radioRequested = false;
    radioPaused = false;
    drawInterface();
    drawStatus("No hay emisoras disponibles");
    Serial.println("Se probaron todas las emisoras");
    return;
  }

  activeStationIndex =
      (activeStationIndex + 1) % static_cast<int8_t>(STATION_COUNT);
  ++stationAttempts;
  radioAttemptStartedAt = millis();
  drawStationList();
  char status[48];
  snprintf(status, sizeof(status), "Probando: %s",
           stations[activeStationIndex].label);
  drawStatus(status);
}

void startRadio(size_t stationIndex) {
  if (radioRequested && activeStationIndex == stationIndex) {
    stopRadio();
    return;
  }

  releaseRadioOutput();
  activeStationIndex = stationIndex;
  radioRequested = true;
  radioPaused = false;
  stationAttempts = 1;
  radioAttemptStartedAt = millis();
  drawInterface();

  if (strlen(WIFI_SSID) == 0) {
    Serial.println("Configura SSID y clave en include/wifi_credentials.h");
    radioRequested = false;
    drawStatus("Configura WiFi");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    drawStatus("Conectando WiFi...");
    return;
  }

  drawStatus("Conectando a la radio...");
}

void beginRadioPlayback() {
  radioAttemptStartedAt = millis();
  radioSource = new AudioFileSourceICYStream(
      stations[activeStationIndex].url);
  if (!radioSource->isOpen()) {
    failoverToNextStation("no responde al conectar");
    return;
  }
  multicastSource = new MulticastAudioSource(radioSource);
  radioBuffer = new AudioFileSourceBuffer(multicastSource, RADIO_BUFFER_SIZE);
  radioOutput = new AudioOutputI2S(0, AudioOutputI2S::INTERNAL_DAC);
  radioOutput->SetOutputModeMono(true);
  radioOutput->SetGain(speakerVolumePercent / 100.0f);
  radioDecoder = new AudioGeneratorMP3();
  audioLogger = &Serial;

  if (!radioDecoder->begin(radioBuffer, radioOutput)) {
    failoverToNextStation("no se pudo iniciar el audio");
    return;
  }

  audioNeedsUnmute = true;
  char status[48];
  snprintf(status, sizeof(status), "Reproduciendo: %s",
           stations[activeStationIndex].label);
  drawStatus(status);
}

void updateRadio() {
  if (!radioRequested || radioPaused) {
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (radioDecoder != nullptr) {
      releaseRadioOutput();
      drawStatus("Reconectando WiFi...");
    }
    return;
  }
  if (radioDecoder == nullptr) {
    beginRadioPlayback();
    return;
  }
  if (radioDecoder->isRunning()) {
    if (!radioDecoder->loop()) {
      failoverToNextStation("se desconecto o no envio audio");
      return;
    }
    if (audioNeedsUnmute &&
        millis() - radioAttemptStartedAt >= RADIO_START_TIMEOUT_MS) {
      failoverToNextStation("tiempo de inicio agotado");
      return;
    }
    unmuteSpeakerAfterAudioStarts();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(SPEAKER_ENABLE_PIN, OUTPUT);
  muteSpeaker();

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  if (!WiFi.softAPConfig(AUDIO_AP_IP, AUDIO_AP_IP, AUDIO_AP_SUBNET)) {
    Serial.println("No se pudo configurar la red WiFi privada de audio");
  } else if (!WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD)) {
    Serial.println("No se pudo iniciar el punto de acceso de audio");
  } else {
    Serial.printf("Red de receptores: %s en %s\n", WIFI_AP_SSID,
                  WiFi.softAPIP().toString().c_str());
    audioMulticastReady = beginAudioMulticast();
    if (!audioMulticastReady) {
      Serial.println("No se pudo iniciar el emisor multicast de audio");
    }
  }
  startControlServer();

  if (strlen(WIFI_SSID) > 0) {
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);

#ifdef TFT_BL
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
#endif

  tft.init();
  tft.setRotation(0);
  tft.setSwapBytes(true);
  Serial.printf("TFT inicializada: %d x %d\n", tft.width(), tft.height());

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(2);

  drawInterface();
}

void loop() {
  controlServer.handleClient();

  const bool isTouching = touchscreen.touched();
  if (isTouching && !touchWasDown) {
    int16_t touchX;
    int16_t touchY;
    if (getTouchPosition(touchX, touchY)) {
      touchStartX = touchX;
      touchStartY = touchY;
      touchLastY = touchY;
      scrollDistance = 0;
      touchMoved = false;
      touchStartedInStationList =
          touchY >= STATION_LIST_Y &&
          touchY < STATION_LIST_Y + STATION_LIST_HEIGHT;

      if (touchY >= TRANSPORT_BUTTON_Y &&
          touchY < TRANSPORT_BUTTON_Y + TRANSPORT_BUTTON_HEIGHT &&
          touchX >= PLAY_BUTTON_X &&
          touchX < PLAY_BUTTON_X + TRANSPORT_BUTTON_WIDTH) {
        playRadio();
      } else if (touchY >= TRANSPORT_BUTTON_Y &&
                 touchY < TRANSPORT_BUTTON_Y + TRANSPORT_BUTTON_HEIGHT &&
                 touchX >= PAUSE_BUTTON_X &&
                 touchX < PAUSE_BUTTON_X + TRANSPORT_BUTTON_WIDTH) {
        pauseRadio();
      } else if (touchY >= VOLUME_BUTTON_Y &&
          touchY < VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT &&
          touchX >= VOLUME_DOWN_X &&
          touchX < VOLUME_DOWN_X + VOLUME_BUTTON_WIDTH) {
        adjustSpeakerVolume(false);
      } else if (touchY >= VOLUME_BUTTON_Y &&
                 touchY < VOLUME_BUTTON_Y + VOLUME_BUTTON_HEIGHT &&
                 touchX >= VOLUME_UP_X &&
                 touchX < VOLUME_UP_X + VOLUME_BUTTON_WIDTH) {
        adjustSpeakerVolume(true);
      }
    }
  } else if (isTouching && touchWasDown && touchStartedInStationList) {
    int16_t touchX;
    int16_t touchY;
    if (getTouchPosition(touchX, touchY)) {
      if (abs(touchY - touchStartY) >= 20 ||
          abs(touchX - touchStartX) >= 20) {
        touchMoved = true;
      }
      scrollDistance += touchY - touchLastY;
      touchLastY = touchY;

      while (scrollDistance >= STATION_BUTTON_SPACING / 2) {
        if (stationScrollOffset > 0) {
          setStationScrollOffset(stationScrollOffset - 1);
        }
        scrollDistance -= STATION_BUTTON_SPACING / 2;
      }
      while (scrollDistance <= -STATION_BUTTON_SPACING / 2) {
        if (stationScrollOffset + VISIBLE_STATION_ROWS <
            STATION_ROW_COUNT) {
          setStationScrollOffset(stationScrollOffset + 1);
        }
        scrollDistance += STATION_BUTTON_SPACING / 2;
      }
    }
  } else if (!isTouching && touchWasDown && touchStartedInStationList &&
             !touchMoved) {
    const size_t visibleRow =
        (touchStartY - STATION_LIST_Y) / STATION_BUTTON_SPACING;
    size_t column = STATION_COLUMN_COUNT;
    for (size_t candidateColumn = 0;
         candidateColumn < STATION_COLUMN_COUNT; ++candidateColumn) {
      const int16_t buttonX = stationButtonX(candidateColumn);
      if (touchStartX >= buttonX &&
          touchStartX < buttonX + STATION_BUTTON_WIDTH) {
        column = candidateColumn;
        break;
      }
    }
    const size_t stationIndex =
        (stationScrollOffset + visibleRow) * STATION_COLUMN_COUNT + column;
    const int16_t stationY =
        STATION_LIST_Y + visibleRow * STATION_BUTTON_SPACING;
    if (column < STATION_COLUMN_COUNT &&
        visibleRow < VISIBLE_STATION_ROWS && stationIndex < STATION_COUNT &&
        touchStartY >= stationY - 2 &&
        touchStartY < stationY + STATION_BUTTON_HEIGHT + 2) {
        startRadio(stationIndex);
    }
  }

  touchWasDown = isTouching;
  if (!isTouching) {
    touchStartedInStationList = false;
  }
  updateRadio();
  if (stationListNeedsRedraw && (!radioRequested || !isTouching)) {
    drawStationListStep();
    updateRadio();
  }
  delay(stationListNeedsRedraw ? 1 : 10);
}
