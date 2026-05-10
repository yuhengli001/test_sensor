#include "vital_signs.h"
#include <stdio.h>
#include <math.h>
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"

typedef struct {
  float real;
  float imag;
} complex_t;

static float distance_history[FFT_N] = {0};
static uint16_t dist_idx = 0;
static bool buffer_full = false;
static uint16_t slide_counter = 0;

static void bit_reverse(complex_t *x, int n) {
  int i, j, k;
  for (i = 1, j = 0; i < n - 1; i++) {
    for (k = n >> 1; (!((j ^= k) & k)); k >>= 1)
      ;
    if (i < j) {
      complex_t temp = x[i];
      x[i] = x[j];
      x[j] = temp;
    }
  }
}

static void compute_fft(complex_t *x, int n) {
  bit_reverse(x, n);
  for (int step = 1; step < n; step <<= 1) {
    float theta = -3.14159265f / step;
    float w_real_step = cosf(theta);
    float w_imag_step = sinf(theta);
    for (int i = 0; i < n; i += 2 * step) {
      float w_real = 1.0f;
      float w_imag = 0.0f;
      for (int j = 0; j < step; j++) {
        int a = i + j;
        int b = i + j + step;
        float t_real = w_real * x[b].real - w_imag * x[b].imag;
        float t_imag = w_real * x[b].imag + w_imag * x[b].real;
        x[b].real = x[a].real - t_real;
        x[b].imag = x[a].imag - t_imag;
        x[a].real += t_real;
        x[a].imag += t_imag;
        float w_real_next = w_real * w_real_step - w_imag * w_imag_step;
        float w_imag_next = w_real * w_imag_step + w_imag * w_real_step;
        w_real = w_real_next;
        w_imag = w_imag_next;
      }
    }
  }
}

static void apply_hamming_window(float *data, int n) {
  for (int i = 0; i < n; i++) {
    data[i] *= (0.54f - 0.46f * cosf(2.0f * 3.14159265f * i / (n - 1)));
  }
}

void vital_signs_init(void) {
    dist_idx = 0;
    buffer_full = false;
    slide_counter = 0;
}

void process_vital_signs(float difference, float current_dist) {
  if (!global_config.enable_vitals_monitoring) return;

  distance_history[dist_idx] = difference;
  dist_idx++;
  if (dist_idx >= FFT_N) {
    dist_idx = 0;
    buffer_full = true;
  }

  slide_counter++;
  if (buffer_full && slide_counter >= 5) {
    slide_counter = 0;
    complex_t fft_data[FFT_N];
    float mean = 0.0f;

    for (int k = 0; k < FFT_N; k++) {
      float val = distance_history[(dist_idx + k) % FFT_N];
      fft_data[k].real = val;
      fft_data[k].imag = 0.0f;
      mean += val;
    }
    mean /= FFT_N;
    for (int k = 0; k < FFT_N; k++) fft_data[k].real -= mean;

    float time_data[FFT_N];
    for (int k = 0; k < FFT_N; k++) time_data[k] = fft_data[k].real;
    apply_hamming_window(time_data, FFT_N);
    for (int k = 0; k < FFT_N; k++) fft_data[k].real = time_data[k];

    compute_fft(fft_data, FFT_N);

    int b_min = (int)(0.1f * FFT_N / SAMPLE_RATE_HZ);
    int b_max = (int)(0.6f * FFT_N / SAMPLE_RATE_HZ);
    float max_b = -1.0f; int peak_b = b_min;
    for (int k = b_min; k <= b_max; k++) {
      float mag = sqrtf(fft_data[k].real * fft_data[k].real + fft_data[k].imag * fft_data[k].imag);
      if (mag > max_b) { max_b = mag; peak_b = k; }
    }

    int h_min = (int)(0.8f * FFT_N / SAMPLE_RATE_HZ);
    int h_max = (int)(3.0f * FFT_N / SAMPLE_RATE_HZ);
    float max_h = -1.0f; int peak_h = h_min;
    float avg_h = 0.0f;
    for (int k = h_min; k <= h_max; k++) {
      float mag = sqrtf(fft_data[k].real * fft_data[k].real + fft_data[k].imag * fft_data[k].imag);
      avg_h += mag;
      if (mag > max_h) { max_h = mag; peak_h = k; }
    }
    avg_h /= (h_max - h_min + 1);

    float freq_b = (float)peak_b * SAMPLE_RATE_HZ / FFT_N;
    float freq_h = (float)peak_h * SAMPLE_RATE_HZ / FFT_N;
    float snr_h = max_h / (avg_h + 0.0001f);
    bool h_ok = (snr_h > 3.0f);

    for (int m = 2; m <= 4; m++) {
      if (fabsf(freq_h - (freq_b * m)) < 0.15f) { h_ok = false; break; }
    }

    if (sys_mode == MODE_NORMAL) {
      printf("[Vitals] Dist: %" PRIfloat "m | Resp: %" PRIfloat " BPM | Heart: ", 
             ACC_LOG_FLOAT_TO_INTEGER(current_dist), ACC_LOG_FLOAT_TO_INTEGER(freq_b * 60.0f));
      if (h_ok) printf("%" PRIfloat " BPM (SNR: %d)\n", ACC_LOG_FLOAT_TO_INTEGER(freq_h * 60.0f), (int)snr_h);
      else printf("[Calculating...]\n");
    }
  }
}
