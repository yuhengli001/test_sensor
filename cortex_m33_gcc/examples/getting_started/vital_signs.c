#include "vital_signs.h"
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"
#include "acc_algorithm.h"
#include "main.h" // Required for BLE headers
#include "vital_sign_service_app.h"

typedef struct {
  float real;
  float imag;
} complex_t;

static float distance_history_b[FFT_N] = {0};
static float distance_history_h[FFT_N] = {0};
static uint16_t dist_idx = 0;
static bool buffer_full = false;
static uint16_t slide_counter = 0;

// 官方巴特沃斯滤波器相关变量 (呼吸)
static float b_coeffs_b[5];
static float a_coeffs_b[4];
static float filter_states_b[5] = {0};

// 官方巴特沃斯滤波器相关变量 (心跳)
static float b_coeffs_h[5];
static float a_coeffs_h[4];
static float filter_states_h[5] = {0};
static bool filter_initialized = false;

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

static float apply_butterworth_b(float input) {
    float output = b_coeffs_b[0] * input + filter_states_b[0];
    filter_states_b[0] = b_coeffs_b[1] * input - a_coeffs_b[0] * output + filter_states_b[1];
    filter_states_b[1] = b_coeffs_b[2] * input - a_coeffs_b[1] * output + filter_states_b[2];
    filter_states_b[2] = b_coeffs_b[3] * input - a_coeffs_b[2] * output + filter_states_b[3];
    filter_states_b[3] = b_coeffs_b[4] * input - a_coeffs_b[3] * output;
    return output;
}

static float apply_butterworth_h(float input) {
    float output = b_coeffs_h[0] * input + filter_states_h[0];
    filter_states_h[0] = b_coeffs_h[1] * input - a_coeffs_h[0] * output + filter_states_h[1];
    filter_states_h[1] = b_coeffs_h[2] * input - a_coeffs_h[1] * output + filter_states_h[2];
    filter_states_h[2] = b_coeffs_h[3] * input - a_coeffs_h[2] * output + filter_states_h[3];
    filter_states_h[3] = b_coeffs_h[4] * input - a_coeffs_h[3] * output;
    return output;
}

void vital_signs_init(void) {
    dist_idx = 0;
    buffer_full = false;
    slide_counter = 0;
    memset(filter_states_b, 0, sizeof(filter_states_b));
    memset(filter_states_h, 0, sizeof(filter_states_h));
    
    // 初始化官方带通滤波器
    acc_algorithm_butter_bandpass(0.1f, 0.6f, SAMPLE_RATE_HZ, b_coeffs_b, a_coeffs_b);
    acc_algorithm_butter_bandpass(0.8f, 3.0f, SAMPLE_RATE_HZ, b_coeffs_h, a_coeffs_h);
    filter_initialized = true;
}

void process_vital_signs(float difference, float current_dist) {
  if (!global_config.enable_vitals_monitoring) return;
  if (!filter_initialized) vital_signs_init();

  // 1. 应用双路独立滤波 (呼吸频段与心跳频段完全隔离)
  float filtered_b = apply_butterworth_b(difference);
  float filtered_h = apply_butterworth_h(difference);

  // 2. 存入历史缓冲区
  distance_history_b[dist_idx] = filtered_b;
  distance_history_h[dist_idx] = filtered_h;
  dist_idx++;
  if (dist_idx >= FFT_N) {
    dist_idx = 0;
    buffer_full = true;
  }

  // 3. 滑动窗口计算
  slide_counter++;
  if (buffer_full && slide_counter >= 10) {
    slide_counter = 0;
    
    // 我们需要两次 FFT: 一次给呼吸，一次给心跳
    complex_t fft_data_b[FFT_N];
    complex_t fft_data_h[FFT_N];
    float psd_b[FFT_N/2 + 1];
    float psd_h[FFT_N/2 + 1];

    for (int k = 0; k < FFT_N; k++) {
      fft_data_b[k].real = distance_history_b[(dist_idx + k) % FFT_N];
      fft_data_b[k].imag = 0.0f;
      
      fft_data_h[k].real = distance_history_h[(dist_idx + k) % FFT_N];
      fft_data_h[k].imag = 0.0f;
    }

    // 官方建议：应用 Hamming 窗以压制频谱泄漏
    float window[FFT_N];
    acc_algorithm_hamming(FFT_N, window);
    for (int k = 0; k < FFT_N; k++) {
        fft_data_b[k].real *= window[k];
        fft_data_h[k].real *= window[k];
    }

    // 计算双路 FFT
    compute_fft(fft_data_b, FFT_N);
    compute_fft(fft_data_h, FFT_N);

    // 计算双路 PSD
    for (int k = 0; k <= FFT_N/2; k++) {
      psd_b[k] = fft_data_b[k].real * fft_data_b[k].real + fft_data_b[k].imag * fft_data_b[k].imag;
      psd_h[k] = fft_data_h[k].real * fft_data_h[k].real + fft_data_h[k].imag * fft_data_h[k].imag;
    }

    // 搜索呼吸区间: 0.1Hz - 0.6Hz
    int b_min = (int)(0.1f * FFT_N / SAMPLE_RATE_HZ);
    int b_max = (int)(0.6f * FFT_N / SAMPLE_RATE_HZ);
    float max_b = -1.0f; 
    int peak_b_idx = b_min;
    for (int k = b_min; k <= b_max; k++) {
      if (psd_b[k] > max_b) { max_b = psd_b[k]; peak_b_idx = k; }
    }

    float freq_delta = SAMPLE_RATE_HZ / FFT_N;
    float freq_b = acc_algorithm_interpolate_peaks_equidistant(psd_b, 0.0f, freq_delta, (uint16_t)peak_b_idx);

    // 搜索心率区间: 0.8Hz - 3.0Hz
    int h_min = (int)(0.8f * FFT_N / SAMPLE_RATE_HZ);
    int h_max = (int)(3.0f * FFT_N / SAMPLE_RATE_HZ);
    float max_h = -1.0f; 
    int peak_h_idx = h_min;
    float avg_h = 0.0f;
    for (int k = h_min; k <= h_max; k++) {
      avg_h += psd_h[k];
      if (psd_h[k] > max_h) { max_h = psd_h[k]; peak_h_idx = k; }
    }
    avg_h /= (h_max - h_min + 1);

    float freq_h = acc_algorithm_interpolate_peaks_equidistant(psd_h, 0.0f, freq_delta, (uint16_t)peak_h_idx);
    
    // 心跳的 SNR 计算 (由于去除了呼吸频率的干扰，SNR 会更纯粹)
    float snr_h = max_h / (avg_h + 0.000001f);
    bool h_ok = (snr_h > 4.0f);

    float bpm_b = freq_b * 60.0f;
    float bpm_h = freq_h * 60.0f;

    // 输出结果
    printf("[Vitals] Dist: %" PRIfloat "m | Resp: %" PRIfloat " BPM | Heart: ", 
           ACC_LOG_FLOAT_TO_INTEGER(current_dist), ACC_LOG_FLOAT_TO_INTEGER(bpm_b));
    
    if (h_ok) {
      printf("%" PRIfloat " BPM (SNR: %d)\n", ACC_LOG_FLOAT_TO_INTEGER(bpm_h), (int)snr_h);
      VITAL_APP_UpdateData(bpm_b, bpm_h, current_dist); // Update Data for BLE
    } else {
      printf("[Calc... Heart SNR: %d]\n", (int)snr_h);
      VITAL_APP_UpdateData(bpm_b, 0.0f, current_dist); // Send 0 for heart rate if SNR is too low
    }

  } else if (!buffer_full) {
    if (dist_idx % 40 == 0) {
      printf("[Vitals] Buffering: %d/%d\n", dist_idx, FFT_N);
    }
  }
}
