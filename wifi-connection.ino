#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>
#include <vector>
#include <cstring>
#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include "HalloweenAnimation.h"
#include "EVADisplayTypes.h"
#include "secrets.h"



const char* serverUrl = "http://10.155.245.40:8000/voice";

// =====================================================
// 2. INMP441 MICROPHONE PINS
// =====================================================

constexpr int MIC_SCK = 14;
constexpr int MIC_WS  = 15;
constexpr int MIC_SD  = 32;

// =====================================================
// 3. MAX98357A SPEAKER PINS
// =====================================================

constexpr int SPK_BCLK = 27;
constexpr int SPK_LRC  = 25;
constexpr int SPK_DIN  = 26;

// Separate I2S controllers for microphone and speaker.
I2SClass mic(I2S_NUM_0);
I2SClass speaker(I2S_NUM_1);

// =====================================================
// 4. AUDIO CONFIGURATION
// =====================================================

constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint16_t BITS_PER_SAMPLE = 16;
constexpr uint16_t NUM_CHANNELS = 1;

constexpr size_t RECORD_SECONDS = 3;
constexpr size_t TARGET_SAMPLES =
    SAMPLE_RATE * RECORD_SECONDS;

constexpr size_t READ_SAMPLES = 256;
constexpr size_t WAV_HEADER_SIZE = 44;

constexpr size_t AUDIO_DATA_SIZE =
    TARGET_SAMPLES * sizeof(int16_t);

constexpr size_t TOTAL_WAV_SIZE =
    WAV_HEADER_SIZE + AUDIO_DATA_SIZE;

// Temporary microphone read buffer.
int32_t rawSamples[READ_SAMPLES];

// =====================================================
// 5. EVA OLED FACE
// =====================================================

// OLED wiring: SDA = GPIO 21, SCL = GPIO 19.
// This constructor was verified by the standalone OLED test.
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
    U8G2_R0, U8X8_PIN_NONE, 19, 21
);


HalloweenAnimation halloween(u8g2);



portMUX_TYPE displayModeMux = portMUX_INITIALIZER_UNLOCKED;

DisplayMode requestedDisplayMode = DISPLAY_FACE;




// Main task uses this function to change display mode.
void requestDisplayMode(DisplayMode mode) {

    portENTER_CRITICAL(&displayModeMux);

    requestedDisplayMode = mode;

    portEXIT_CRITICAL(&displayModeMux);
}


// OLED task uses this function to read display mode.
DisplayMode getRequestedDisplayMode() {

    portENTER_CRITICAL(&displayModeMux);

    DisplayMode mode = requestedDisplayMode;

    portEXIT_CRITICAL(&displayModeMux);

    return mode;
}



// Shared state: changed by the main task, read by the OLED task.
volatile EvaState evaState = EVA_IDLE;
TaskHandle_t oledTaskHandle = nullptr;

void setEvaState(EvaState newState) {
  evaState = newState;
}

void drawEye(int x, int eyeY, int eyeHeight,
             int pupilX, int pupilY) {
  // Bright eye with a dark pupil and a small highlight.
  u8g2.setDrawColor(1);
  u8g2.drawRBox(x, eyeY, 30, eyeHeight, 8);

  const int centerY = eyeY + eyeHeight / 2;

  u8g2.setDrawColor(0);
  u8g2.drawDisc(x + 15 + pupilX, centerY + pupilY, 5);

  u8g2.setDrawColor(1);
  u8g2.drawDisc(x + 12 + pupilX, centerY - 3 + pupilY, 2);
}

void drawBlink() {
  u8g2.setDrawColor(1);
  u8g2.drawRBox(22, 21, 30, 3, 1);
  u8g2.drawRBox(76, 21, 30, 3, 1);
}

void drawIdleBrows() {
  // Soft, gently arched brows make EVA's resting expression friendlier.
  u8g2.setDrawColor(1);
  u8g2.drawLine(23, 8, 29, 4);
  u8g2.drawLine(29, 4, 36, 3);
  u8g2.drawLine(36, 3, 43, 4);
  u8g2.drawLine(43, 4, 50, 8);

  u8g2.drawLine(78, 8, 85, 4);
  u8g2.drawLine(85, 4, 92, 3);
  u8g2.drawLine(92, 3, 99, 4);
  u8g2.drawLine(99, 4, 105, 8);
}

void drawIdleSmile() {
  u8g2.setDrawColor(1);

  // Tiny cheek marks add warmth without crowding the 128x64 screen.
  u8g2.drawPixel(14, 43);
  u8g2.drawPixel(17, 45);
  u8g2.drawPixel(20, 43);
  u8g2.drawPixel(108, 43);
  u8g2.drawPixel(111, 45);
  u8g2.drawPixel(114, 43);

  // A small curved smile built from line segments (U-shaped).
  u8g2.drawLine(51, 44, 55, 47);
  u8g2.drawLine(51, 45, 55, 48);
  u8g2.drawLine(55, 47, 59, 49);
  u8g2.drawLine(55, 48, 59, 50);
  u8g2.drawLine(59, 49, 64, 50);
  u8g2.drawLine(59, 50, 64, 51);
  u8g2.drawLine(64, 50, 69, 49);
  u8g2.drawLine(64, 51, 69, 50);
  u8g2.drawLine(69, 49, 73, 47);
  u8g2.drawLine(69, 50, 73, 48);
  u8g2.drawLine(73, 47, 77, 44);
  u8g2.drawLine(73, 48, 77, 45);
}

void drawListeningEyes() {
  // Wider, attentive eyes while the microphone is recording.
  drawEye(22, 4, 35, 0, 1);
  drawEye(76, 4, 35, 0, 1);

  // Small attention marks on the outside of both eyes.
  u8g2.setDrawColor(1);
  u8g2.drawLine(14, 13, 18, 15);
  u8g2.drawLine(13, 22, 18, 22);
  u8g2.drawLine(14, 31, 18, 29);

  u8g2.drawLine(110, 15, 114, 13);
  u8g2.drawLine(110, 22, 115, 22);
  u8g2.drawLine(110, 29, 114, 31);
}

void drawThinkingEyes() {
  // Narrower eyes and raised, angled brows make the expression
  // visibly different from EVA's relaxed idle face.
  drawEye(22, 12, 24, 4, -4);
  drawEye(76, 12, 24, 4, -4);

  u8g2.setDrawColor(1);
  // Brows angled toward the center as if concentrating.
  u8g2.drawLine(22, 8, 36, 4);
  u8g2.drawLine(36, 4, 50, 8);
  u8g2.drawLine(78, 8, 92, 4);
  u8g2.drawLine(92, 4, 106, 8);
}

void drawSpeakingAnimation(unsigned long now) {
  // A lively audio waveform replaces the SPEAKING label.
  const int baseline = 58;

  for (int i = 0; i < 9; i++) {
    const float phase = now / 105.0f + i * 0.8f;
    const int height = 3 + static_cast<int>(
        5.0f * (1.0f + sinf(phase))
    );
    const int x = 35 + i * 7;
    u8g2.drawVLine(x, baseline - height, height);
  }
}

void drawThinkingAnimation(unsigned long now) {
  // Three pulsing thought dots below EVA's concentrated expression.
  const int activeDot = (now / 240) % 3;

  for (int i = 0; i < 3; i++) {
    const int radius = (i == activeDot) ? 2 : 1;
    u8g2.drawDisc(56 + i * 8, 46, radius);
  }
}

void drawFace(unsigned long now, EvaState currentState, bool blinking) {
  u8g2.clearBuffer();

  if (blinking) {
    // Keep blinking simple and consistent across states.
    drawBlink();
  } else {
    switch (currentState) {
      case EVA_LISTENING:
        drawListeningEyes();
        break;

      case EVA_THINKING:
        drawThinkingEyes();
        break;

      case EVA_SPEAKING:
        drawEye(22, 7, 29, 0, 0);
        drawEye(76, 7, 29, 0, 0);
        break;

      case EVA_IDLE:
      default: {
        // Relaxed eyes gently glance left and right while idle.
        const float gazePhase = sinf(static_cast<float>(now) / 2200.0f);
        const int pupilX = (gazePhase > 0.55f)
                               ? 1
                               : ((gazePhase < -0.55f) ? -1 : 0);
        drawEye(22, 8, 28, pupilX, 0);
        drawEye(76, 8, 28, pupilX, 0);
        break;
      }
    }
  }

  // Keep brows and smile visible during a blink, so the idle face
  // remains expressive even while the eyelids are closed.
  if (currentState == EVA_IDLE) {
    drawIdleBrows();
    drawIdleSmile();
  }

  // Speaking uses the waveform instead of a text label.
  // Idle is intentionally label-free. Keep LISTENING and THINKING
  // as small cues while their expressions make the state recognizable.
  if (currentState == EVA_SPEAKING) {
    drawSpeakingAnimation(now);
  } else if (currentState == EVA_THINKING) {
    drawThinkingAnimation(now);
  }

  const char* label = nullptr;
  if (currentState == EVA_LISTENING) {
    label = "LISTENING";
  } else if (currentState == EVA_THINKING) {
    label = "THINKING";
  }

  if (label != nullptr) {
    u8g2.setFont(u8g2_font_6x10_tf);
    const int textWidth = u8g2.getStrWidth(label);
    u8g2.drawStr((128 - textWidth) / 2, 62, label);
  }

  u8g2.sendBuffer();
}

// Run display updates independently so blocking HTTP/audio operations
// do not freeze EVA's face animation.

void oledAnimationTask(void* parameter) {

    (void)parameter;

    bool blinking = false;

    unsigned long blinkStartedAt = 0;
    unsigned long nextBlinkAt = millis() + random(1800, 4000);
    unsigned long lastFrameAt = 0;

    // Only this task owns the active display mode.
    DisplayMode activeMode = DISPLAY_FACE;

    unsigned long halloweenStartedAt = 0;

    // Return to normal EVA after 12 seconds.
    constexpr unsigned long HALLOWEEN_DURATION = 12000;

    for (;;) {

        const unsigned long now = millis();

        // -----------------------------------------
        // BLINKING
        // -----------------------------------------

        if (!blinking &&
            static_cast<long>(now - nextBlinkAt) >= 0) {

            blinking = true;
            blinkStartedAt = now;
        }

        if (blinking && now - blinkStartedAt >= 140) {

            blinking = false;
            nextBlinkAt = now + random(1800, 4000);
        }

        // -----------------------------------------
        // DISPLAY UPDATE
        // -----------------------------------------

        if (now - lastFrameAt >= 33) {

            // Read requested display mode.
            const DisplayMode requested = getRequestedDisplayMode();

            // Detect display mode changes.
            if (requested != activeMode) {

                activeMode = requested;

                if (activeMode == DISPLAY_HALLOWEEN) {

                    halloween.start(now);
                    halloweenStartedAt = now;

                    Serial.println(
                        "EVA: Halloween mode activated!"
                    );
                }
            }

            // Automatically return to normal face.
            if (
                activeMode == DISPLAY_HALLOWEEN &&
                now - halloweenStartedAt >= HALLOWEEN_DURATION
            ) {

                activeMode = DISPLAY_FACE;
                requestDisplayMode(DISPLAY_FACE);

                Serial.println(
                    "EVA: Returning to normal display."
                );
            }

            // -------------------------------------
            // RENDER DISPLAY
            // -------------------------------------

            if (activeMode == DISPLAY_HALLOWEEN) {

                u8g2.clearBuffer();

                halloween.draw(now);

                u8g2.sendBuffer();

            } else {

                const EvaState currentState = evaState;

                drawFace(
                    now,
                    currentState,
                    blinking
                );
            }

            lastFrameAt = now;
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}


// =====================================================
// 6. LITTLE-ENDIAN HELPERS
// =====================================================

void writeLE16(uint8_t* p, uint16_t value) {
  p[0] = static_cast<uint8_t>(value & 0xFF);
  p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
}

void writeLE32(uint8_t* p, uint32_t value) {
  p[0] = static_cast<uint8_t>(value & 0xFF);
  p[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
  p[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
  p[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

uint16_t readLE16(const uint8_t* p) {
  return static_cast<uint16_t>(p[0]) |
         (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t readLE32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) |
         (static_cast<uint32_t>(p[1]) << 8) |
         (static_cast<uint32_t>(p[2]) << 16) |
         (static_cast<uint32_t>(p[3]) << 24);
}

// =====================================================
// 7. WAV HEADER GENERATION
// =====================================================

void writeWavHeader(uint8_t* header, uint32_t dataSize) {
  memset(header, 0, WAV_HEADER_SIZE);

  // RIFF chunk
  memcpy(header, "RIFF", 4);
  writeLE32(header + 4, 36 + dataSize);
  memcpy(header + 8, "WAVE", 4);

  // Format chunk
  memcpy(header + 12, "fmt ", 4);
  writeLE32(header + 16, 16);  // PCM fmt chunk size
  writeLE16(header + 20, 1);   // PCM format
  writeLE16(header + 22, NUM_CHANNELS);
  writeLE32(header + 24, SAMPLE_RATE);

  const uint32_t byteRate =
      SAMPLE_RATE * NUM_CHANNELS * BITS_PER_SAMPLE / 8;

  const uint16_t blockAlign =
      NUM_CHANNELS * BITS_PER_SAMPLE / 8;

  writeLE32(header + 28, byteRate);
  writeLE16(header + 32, blockAlign);
  writeLE16(header + 34, BITS_PER_SAMPLE);

  // Audio data chunk
  memcpy(header + 36, "data", 4);
  writeLE32(header + 40, dataSize);
}

// =====================================================
// 8. WI-FI CONNECTION
// =====================================================

bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to Wi-Fi...");

  const unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED &&
         millis() - start < 30000UL) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi connection failed.");
    return false;
  }

  Serial.println("Wi-Fi connected.");
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  Serial.print("Backend URL: ");
  Serial.println(serverUrl);

  return true;
}

// =====================================================
// 9. WAV RESPONSE VALIDATION
// =====================================================


bool validateWavHeader(
    const uint8_t* header,
    uint16_t& channels,
    uint32_t& sampleRate,
    uint16_t& bitsPerSample,
    uint32_t& dataSize) {

  // Validate the expected 44-byte PCM WAV header layout.
  if (memcmp(header, "RIFF", 4) != 0 ||
      memcmp(header + 8, "WAVE", 4) != 0 ||
      memcmp(header + 12, "fmt ", 4) != 0 ||
      memcmp(header + 36, "data", 4) != 0) {
    Serial.println("ERROR: Unsupported WAV header layout.");
    return false;
  }

  const uint32_t riffSize = readLE32(header + 4);
  const uint32_t formatChunkSize = readLE32(header + 16);
  const uint16_t format = readLE16(header + 20);

  channels = readLE16(header + 22);
  sampleRate = readLE32(header + 24);
  const uint32_t byteRate = readLE32(header + 28);
  const uint16_t blockAlign = readLE16(header + 32);
  bitsPerSample = readLE16(header + 34);
  dataSize = readLE32(header + 40);

  // Print the values from the actual header being validated.
  Serial.printf(
      "WAV header: format=%u, channels=%u, "
      "sampleRate=%lu, bits=%u\n",
      format,
      channels,
      static_cast<unsigned long>(sampleRate),
      bitsPerSample
  );

  Serial.printf(
      "WAV sizes: RIFF=%lu, data=%lu, fmt=%lu\n",
      static_cast<unsigned long>(riffSize),
      static_cast<unsigned long>(dataSize),
      static_cast<unsigned long>(formatChunkSize)
  );

  Serial.printf(
      "WAV alignment: blockAlign=%u, byteRate=%lu\n",
      blockAlign,
      static_cast<unsigned long>(byteRate)
  );

  // Validate standard uncompressed PCM.
  if (formatChunkSize != 16 ||
      format != 1 ||
      (channels != 1 && channels != 2) ||
      bitsPerSample != 16 ||
      sampleRate == 0 ||
      dataSize == 0) {
    Serial.println("ERROR: Unsupported WAV audio format.");
    return false;
  }

  const uint16_t expectedBlockAlign =
      channels * (bitsPerSample / 8);

  const uint64_t expectedByteRate =
      static_cast<uint64_t>(sampleRate) * expectedBlockAlign;

  // Check the PCM format fields and audio data alignment.
  if (blockAlign != expectedBlockAlign ||
      expectedByteRate > UINT32_MAX ||
      byteRate != expectedByteRate ||
      dataSize % expectedBlockAlign != 0) {
    Serial.println("ERROR: Inconsistent WAV format fields.");
    return false;
  }

  // A standard 44-byte WAV header requires RIFF size = 36 + data size.
  if (static_cast<uint64_t>(dataSize) + 36 != riffSize) {
    Serial.println("ERROR: Inconsistent RIFF and data sizes.");
    return false;
  }

  return true;
}


// =====================================================
// 10. RECORD, UPLOAD, AND PLAY RESPONSE
// =====================================================

void recordAndTranscribe() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERROR: Wi-Fi is not connected.");
    return;
  }

  // Allocate the complete recording buffer once.
  std::vector<uint8_t> wav;

  if (ESP.getFreeHeap() < TOTAL_WAV_SIZE + 12000) {
    Serial.println("ERROR: Insufficient free heap.");
    return;
  }

  wav.resize(TOTAL_WAV_SIZE, 0);

  size_t samplesCaptured = 0;

  Serial.println();
  Serial.println("Get ready...");
  delay(700);

  setEvaState(EVA_LISTENING);
  Serial.println("Recording for 2 seconds. Speak now!");

  // ---------------------------------------------------
  // Capture microphone samples.
  // ---------------------------------------------------

  while (samplesCaptured < TARGET_SAMPLES) {
    size_t bytesRead = mic.readBytes(
        reinterpret_cast<char*>(rawSamples),
        sizeof(rawSamples)
    );

    // Only process complete 32-bit samples.
    size_t count = bytesRead / sizeof(int32_t);

    if (count == 0) {
      delay(1);
      continue;
    }

    const size_t remainingSamples =
        TARGET_SAMPLES - samplesCaptured;

    const size_t toCopy =
        count < remainingSamples ? count : remainingSamples;

    for (size_t i = 0; i < toCopy; i++) {
      // Assumes the useful microphone data occupies the
      // upper 16 bits of the I2S sample.
      const int16_t sample =
          static_cast<int16_t>(rawSamples[i] >> 16);

      const uint16_t value =
          static_cast<uint16_t>(sample);

      const size_t offset =
          WAV_HEADER_SIZE + (samplesCaptured + i) * 2;

      wav[offset] =
          static_cast<uint8_t>(value & 0xFF);

      wav[offset + 1] =
          static_cast<uint8_t>((value >> 8) & 0xFF);
    }

    samplesCaptured += toCopy;
  }

  // Recording is complete; upload and AI processing begin now.
  writeWavHeader(wav.data(), AUDIO_DATA_SIZE);
  setEvaState(EVA_THINKING);

  Serial.printf(
      "Captured samples: %u / %u\n",
      static_cast<unsigned>(samplesCaptured),
      static_cast<unsigned>(TARGET_SAMPLES)
  );

  Serial.printf(
      "Recording size: %u bytes\n",
      static_cast<unsigned>(wav.size())
  );

  Serial.printf(
      "Free heap before upload: %u bytes\n",
      ESP.getFreeHeap()
  );

  // ---------------------------------------------------
  // Upload recording to the laptop backend.
  // ---------------------------------------------------

  Serial.println("Uploading audio to EVA...");

  WiFiClient client;
  HTTPClient http;

  http.setConnectTimeout(10000);
  http.setTimeout(60000);

  if (!http.begin(client, serverUrl)) {
    Serial.println("ERROR: Could not initialize HTTP.");
    setEvaState(EVA_IDLE);
    return;
  }

  http.addHeader("Content-Type", "audio/wav");
  http.addHeader("Accept", "audio/wav");
  const char* responseHeaders[] = {
    "X-EVA-Display-Mode"
  };

  http.collectHeaders(responseHeaders, 1);
  const int statusCode = http.POST(wav.data(), wav.size());

  Serial.printf("HTTP status: %d\n", statusCode);

  if (statusCode != HTTP_CODE_OK) {
    if (statusCode > 0) {
      Serial.println("Backend error response:");
      Serial.println(http.getString());
    } else {
      Serial.print("HTTP request failed: ");
      Serial.println(http.errorToString(statusCode));
    }

    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

  // ---------------------------------------------------
  // Validate response length and WAV header.
  // ---------------------------------------------------

  const int contentLength = http.getSize();
  WiFiClient* responseStream = http.getStreamPtr();

  if (contentLength <= static_cast<int>(WAV_HEADER_SIZE) ||
      responseStream == nullptr) {
    Serial.println(
        "ERROR: Missing WAV response or Content-Length."
    );
    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

  uint8_t responseHeader[WAV_HEADER_SIZE];

  const size_t headerRead = responseStream->readBytes(
      reinterpret_cast<char*>(responseHeader),
      sizeof(responseHeader)
  );

  if (headerRead != sizeof(responseHeader)) {
    Serial.println("ERROR: Could not read WAV header.");
    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

  uint16_t channels = 0;
  uint32_t responseSampleRate = 0;
  uint16_t responseBitsPerSample = 0;
  uint32_t responseDataSize = 0;

  if (!validateWavHeader(
          responseHeader,
          channels,
          responseSampleRate,
          responseBitsPerSample,
          responseDataSize)) {
    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

  const uint32_t expectedResponseDataSize =
      static_cast<uint32_t>(contentLength - WAV_HEADER_SIZE);

  if (responseDataSize != expectedResponseDataSize) {
    Serial.println(
        "ERROR: WAV data size does not match Content-Length."
    );
    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

  Serial.printf(
      "EVA audio: %lu Hz, %u channel(s), %u-bit\n",
      static_cast<unsigned long>(responseSampleRate),
      channels,
      responseBitsPerSample
  );

  // ---------------------------------------------------
  // Initialize speaker I2S.
  // ---------------------------------------------------

  speaker.setPins(SPK_BCLK, SPK_LRC, SPK_DIN, -1);

  const auto slotMode =
      channels == 1
          ? I2S_SLOT_MODE_MONO
          : I2S_SLOT_MODE_STEREO;

  if (!speaker.begin(
          I2S_MODE_STD,
          responseSampleRate,
          I2S_DATA_BIT_WIDTH_16BIT,
          slotMode)) {
    Serial.println("ERROR: Speaker I2S initialization failed.");
    http.end();
    setEvaState(EVA_IDLE);
    return;
  }

// Read the display instruction from Python.
String receivedMode = http.header(
    "X-EVA-Display-Mode"
);

// Debug HTTP response and display mode.
Serial.println("========== DISPLAY DEBUG ==========");

Serial.print("Received mode: [");
Serial.print(receivedMode);
Serial.println("]");

Serial.print("HTTP status: ");
Serial.println(statusCode);

Serial.println("====================================");

// Request the appropriate OLED experience.
if (receivedMode == "halloween") {
    requestDisplayMode(DISPLAY_HALLOWEEN);
} else {
    requestDisplayMode(DISPLAY_FACE);
}


// Request the appropriate OLED experience.
  if (receivedMode == "halloween") {

    requestDisplayMode(DISPLAY_HALLOWEEN);

  } else {

    requestDisplayMode(DISPLAY_FACE);

  }
  // ---------------------------------------------------
  // Stream audio directly to the speaker.
  // ---------------------------------------------------

  setEvaState(EVA_SPEAKING);
  Serial.println("Playing EVA's response...");

  uint8_t audioBuffer[2048];
  uint32_t remaining = responseDataSize;
  bool playbackOK = true;

  while (remaining > 0) {
    const size_t wanted =
        remaining < sizeof(audioBuffer)
            ? static_cast<size_t>(remaining)
            : sizeof(audioBuffer);

    const size_t received = responseStream->readBytes(
        reinterpret_cast<char*>(audioBuffer),
        wanted
    );

    if (received == 0) {
      Serial.println("ERROR: Audio stream ended unexpectedly.");
      playbackOK = false;
      break;
    }

    size_t offset = 0;

    while (offset < received) {
      const size_t written = speaker.write(
          audioBuffer + offset,
          received - offset
      );

      if (written == 0) {
        Serial.println("ERROR: Speaker write failed.");
        playbackOK = false;
        break;
      }

      offset += written;
    }

    if (!playbackOK) {
      break;
    }

    remaining -= static_cast<uint32_t>(received);
  }

  // Release I2S and HTTP resources.
  speaker.end();
  http.end();
  setEvaState(EVA_IDLE);

  if (playbackOK && remaining == 0) {
    Serial.println("EVA finished speaking!");
  } else {
    Serial.println("EVA playback did not complete.");
  }

  Serial.printf(
      "Free heap after request: %u bytes\n",
      ESP.getFreeHeap()
  );
}

// =====================================================
// 11. SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("==================================");
  Serial.println(" EVA - ESP32 Voice Assistant + OLED");
  Serial.println("==================================");

  // Initialize the OLED face on SDA=21, SCL=19.
  u8g2.setI2CAddress(0x3C << 1);
  u8g2.begin();
  randomSeed(micros());
  drawFace(millis(), EVA_IDLE, false);

  // Use a dedicated low-priority task so display animation keeps
  // updating while the main task waits for Wi-Fi/HTTP/audio I/O.
  BaseType_t taskResult = xTaskCreatePinnedToCore(
      oledAnimationTask,
      "EVA_OLED",
      4096,
      nullptr,
      1,
      &oledTaskHandle,
      0
  );

  if (taskResult != pdPASS) {
    Serial.println("WARNING: OLED animation task failed to start.");
    oledTaskHandle = nullptr;
  }

  // Initialize INMP441 microphone.
  mic.setPins(MIC_SCK, MIC_WS, -1, MIC_SD);

  if (!mic.begin(
          I2S_MODE_STD,
          SAMPLE_RATE,
          I2S_DATA_BIT_WIDTH_32BIT,
          I2S_SLOT_MODE_MONO)) {
    Serial.println(
        "ERROR: Microphone I2S initialization failed."
    );

    while (true) {
      delay(1000);
    }
  }

  Serial.println("INMP441 initialized.");

  // Connect to Wi-Fi.
  if (!connectWiFi()) {
    Serial.println(
        "Wi-Fi failed. Restart after checking credentials."
    );

    while (true) {
      delay(1000);
    }
  }
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();
  setEvaState(EVA_IDLE);
  Serial.println("EVA is ready!");
  Serial.println("Send 'r' in Serial Monitor to record.");
  Serial.println("OLED states change automatically during each request.");
}

// =====================================================
// 12. MAIN LOOP
// =====================================================

void loop() {
  if (Serial.available() > 0) {
    const char command = Serial.read();

    if (command == 'r' || command == 'R') {
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Wi-Fi disconnected; reconnecting...");

        if (!connectWiFi()) {
          setEvaState(EVA_IDLE);
          Serial.println("Request cancelled.");
          return;
        }
      }

      recordAndTranscribe();

      Serial.println();
      Serial.println("Ready! Send 'r' to record again.");
    }
  }

  delay(10);
}
