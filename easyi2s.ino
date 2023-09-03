#include <driver/i2s.h>

class EasyI2S {
  private:
    int ws, sd, sck;
    int bitsPerSample;
    int sampleRate;
    const int bufferLen = 64;
    int16_t sBuffer[64];

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

  public:
    EasyI2S(int ws, int sd, int sck, int bitsPerSample, int sampleRate) 
    : ws(ws), sd(sd), sck(sck), bitsPerSample(bitsPerSample), sampleRate(sampleRate) {
      install();
      setPin();
    }

    void start() {
      i2s_start(I2S_NUM_0);
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

    void stop() {
      i2s_stop(I2S_NUM_0);
    }
};

EasyI2S microphone(25, 26, 33, 16, 44100);

void setup() {
  Serial.begin(115200);
  Serial.println(" ");
  delay(1000);

  microphone.start();
  delay(500);
}

void loop() {
  int rangelimit = 3000;
  Serial.print(rangelimit * -1);
  Serial.print(" ");
  Serial.print(rangelimit);
  Serial.print(" ");

  float mean = microphone.read();
  Serial.println(mean);
}
