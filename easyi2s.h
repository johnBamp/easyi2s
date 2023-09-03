#ifndef EASYI2S_H
#define EASYI2S_H

#include <driver/i2s.h>
#include <SdFat.h>

SdFat SD;
const int chipSelect = 15;  // Change this if your SD module uses a different pin

class EasyI2S {
  private:

    int ws, sd, sck;
    int bitsPerSample;
    int sampleRate;
    const int bufferLen = 64;
    int16_t sBuffer[64];

    File* rawFile = nullptr;
    uint32_t totalBytesRecorded = 0;

    bool isStarted = false;

    int16_t audioBuffer[12288];

    SdFat& SD;

    void install() {
      const i2s_config_t i2s_config = {
        .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = sampleRate,
        .bits_per_sample = i2s_bits_per_sample_t(bitsPerSample),
        .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = bufferLen,
        .use_apll = false
      };

      i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    }

    void setPin() {
      const i2s_pin_config_t pin_config = {
        .bck_io_num = sck,
        .ws_io_num = ws,
        .data_out_num = -1,
        .data_in_num = sd
      };

      i2s_set_pin(I2S_NUM_0, &pin_config);
    }

    struct WavHeader {
      char riffID[4];       // "RIFF"
      uint32_t riffSize;
      char waveID[4];       // "WAVE"
      char fmtID[4];        // "fmt "
      uint32_t fmtSize;
      uint16_t audioFormat;
      uint16_t numChannels;
      uint32_t sampleRate;
      uint32_t byteRate;
      uint16_t blockAlign;
      uint16_t bitsPerSample;
      char dataID[4];       // "data"
      uint32_t dataSize;
    };

    void initializeWavHeader(WavHeader& header, uint32_t numSamples, uint16_t bitsPerSample, uint32_t sampleRate) {
        header.riffID[0] = 'R'; header.riffID[1] = 'I'; header.riffID[2] = 'F'; header.riffID[3] = 'F';
        header.riffSize = numSamples * bitsPerSample / 8 + 36;
        header.waveID[0] = 'W'; header.waveID[1] = 'A'; header.waveID[2] = 'V'; header.waveID[3] = 'E';
        header.fmtID[0] = 'f'; header.fmtID[1] = 'm'; header.fmtID[2] = 't'; header.fmtID[3] = ' ';
        header.fmtSize = 16;
        header.audioFormat = 1;  // PCM
        header.numChannels = 1;  // Mono
        header.sampleRate = sampleRate;
        header.byteRate = sampleRate * bitsPerSample / 8;
        header.blockAlign = bitsPerSample / 8;
        header.bitsPerSample = bitsPerSample;
        header.dataID[0] = 'd'; header.dataID[1] = 'a'; header.dataID[2] = 't'; header.dataID[3] = 'a';
        header.dataSize = numSamples * bitsPerSample / 8;
    }

  public:
    EasyI2S(int ws, int sd, int sck, int bitsPerSample, int sampleRate, SdFat& sdInstance) 
    : ws(ws), sd(sd), sck(sck), bitsPerSample(bitsPerSample), sampleRate(sampleRate), SD(sdInstance) {
      install();
      setPin();
    }

    void start() {
      i2s_start(I2S_NUM_0);
      isStarted = true;
    }

    float read() {
      size_t bytesIn = 0;
      float mean = 0.0;
      esp_err_t result = i2s_read(I2S_NUM_0, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

      if (result == ESP_OK) {
        int16_t samples_read = bytesIn / (bitsPerSample / 8); 
        if (samples_read > 0) {
          for (int16_t i = 0; i < samples_read; ++i) {
            mean += sBuffer[i];
          }
          mean /= samples_read;
        }
      }
      return mean;
    }

    void readRaw(int16_t* buffer, size_t& bytesRead) {
      size_t bytesIn = 0;
      esp_err_t result = i2s_read(I2S_NUM_0, buffer, bufferLen, &bytesIn, portMAX_DELAY);
      bytesRead = bytesIn;
    }

    void stop() {
      i2s_stop(I2S_NUM_0);
      isStarted = false;
    }

    void startRecord(const char* filename) {

      rawFile = new File(SD.open(filename, FILE_WRITE));
      if (!rawFile || !rawFile->isFile()) {
          Serial.println("Error opening file for write");
          return;
      }

      totalBytesRecorded = 0;

      start();
    }

    void record() {
      if (!rawFile) return;

      size_t bytesRead;
      readRaw(audioBuffer, bytesRead);
      rawFile->write((byte *)audioBuffer, bytesRead);
      totalBytesRecorded += bytesRead;
    }

    void stopRecord() {
      stop();
      
      // Assuming 16-bit samples and 44100Hz sampling rate
      WavHeader wavHeader;
      initializeWavHeader(wavHeader, totalBytesRecorded / 2, 16, 44100);
      
      const char* wavFilename = "audio.wav";
      File wavFile = SD.open(wavFilename, FILE_WRITE);
      if (!wavFile) {
          Serial.println("Error opening audio.wav for write");
          return;
      }
      wavFile.write((byte *)&wavHeader, sizeof(WavHeader));

      // Reset raw file to start position and append raw audio data to the WAV file
      rawFile->seek(0);
      size_t bytesRead;
      while (rawFile->available()) {
          bytesRead = rawFile->read(audioBuffer, sizeof(audioBuffer));
          wavFile.write((byte *)audioBuffer, bytesRead);
      }
      
      rawFile->close();
      delete rawFile;
      rawFile = nullptr;

      SD.remove("temp.raw"); // remove the temporary raw file
      wavFile.close();

      Serial.println("Recording finished and saved to audio.wav!");
    }
};

#endif
