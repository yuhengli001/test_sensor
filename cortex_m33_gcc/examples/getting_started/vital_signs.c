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

// 精测 FFT 工作区
static complex_t s_fft_b[FFT_N];
static complex_t s_fft_h[FFT_N];
static float     s_psd_b[FFT_N / 2 + 1];
static float     s_psd_h[FFT_N / 2 + 1];
static float     s_window[FFT_N];

// 粗扫状态（约 2.5KB 额外静态 RAM）
static float     coarse_buf[COARSE_MAX_CANDS][COARSE_N];
static int       coarse_fill;
static bool      coarse_active;
static complex_t coarse_fft_work[COARSE_N]; // 粗扫专用 FFT 工作区，128点×8B=1KB

// 高斯插值峰值定位：比抛物线插值更适合周期信号的频谱峰形状
// 在对数域拟合，等价于假设峰形为高斯曲线
static float gaussian_peak_interp(const float *psd, int k, int k_min, int k_max, float freq_delta) {
    if (k <= k_min || k >= k_max) return k * freq_delta;
    float a = logf(psd[k - 1] + 1e-30f);
    float b = logf(psd[k]     + 1e-30f);
    float c = logf(psd[k + 1] + 1e-30f);
    float denom = a - 2.0f * b + c;
    if (fabsf(denom) < 1e-20f) return k * freq_delta;
    float delta = 0.5f * (a - c) / denom;
    // delta 正常范围是 [-0.5, 0.5]，超出说明峰不在这里，退化为中心 bin
    if (delta < -0.5f || delta > 0.5f) return k * freq_delta;
    return (k + delta) * freq_delta;
}

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

// -----------------------------------------------------------------------
// 粗扫内部：对某个 slot 的 COARSE_N 帧数据做 FFT，返回指定频段的峰值 SNR
// -----------------------------------------------------------------------
static float compute_coarse_snr(int slot, float f_lo, float f_hi) {
    // 去均值（去 DC），避免直流分量泄漏污染低频段
    float mean = 0.0f;
    for (int k = 0; k < COARSE_N; k++) mean += coarse_buf[slot][k];
    mean /= COARSE_N;

    for (int k = 0; k < COARSE_N; k++) {
        coarse_fft_work[k].real = coarse_buf[slot][k] - mean;
        coarse_fft_work[k].imag = 0.0f;
    }
    compute_fft(coarse_fft_work, COARSE_N);

    float freq_res = SAMPLE_RATE_HZ / COARSE_N;
    int k_lo = (int)(f_lo / freq_res);
    int k_hi = (int)(f_hi / freq_res);
    if (k_hi >= COARSE_N / 2) k_hi = COARSE_N / 2 - 1;

    float max_p = 0.0f, sum_p = 0.0f;
    int cnt = k_hi - k_lo + 1;
    for (int k = k_lo; k <= k_hi; k++) {
        float p = coarse_fft_work[k].real * coarse_fft_work[k].real
                + coarse_fft_work[k].imag * coarse_fft_work[k].imag;
        sum_p += p;
        if (p > max_p) max_p = p;
    }
    float avg_p = (cnt > 0) ? sum_p / cnt : 0.0f;
    return max_p / (avg_p + 1e-30f);
}

// -----------------------------------------------------------------------
// 粗扫公开接口
// -----------------------------------------------------------------------
void vital_signs_coarse_start(void) {
    memset(coarse_buf, 0, sizeof(coarse_buf));
    coarse_fill  = 0;
    coarse_active = true;
}

void vital_signs_coarse_feed(int slot, float angle) {
    if (!coarse_active || slot < 0 || slot >= COARSE_MAX_CANDS) return;
    if (coarse_fill < COARSE_N) {
        coarse_buf[slot][coarse_fill] = angle;
    }
}

bool vital_signs_coarse_tick(void) {
    if (!coarse_active) return false;
    coarse_fill++;
    return (coarse_fill >= COARSE_N);
}

int vital_signs_coarse_pick_best(int n_candidates) {
    coarse_active = false;
    int   best_slot  = -1;
    float best_score = 0.0f;

    for (int s = 0; s < n_candidates; s++) {
        float r_snr = compute_coarse_snr(s, 0.1f, 0.5f);
        float h_snr = compute_coarse_snr(s, 0.9f,  3.0f);
        printf("[Coarse] Slot %d: Resp SNR=%.1f  Heart SNR=%.1f\n", s, r_snr, h_snr);
        // 两个信号都必须高于阈值，score 越高越好
        if (r_snr > 2.0f && h_snr > 2.0f) {
            float score = r_snr + h_snr;
            if (score > best_score) {
                best_score = score;
                best_slot  = s;
            }
        }
    }
    return best_slot; // -1 表示没有找到合适位置
}

// 把粗扫数据回放进精测滤波器，缩短 FFT 首次触发等待时间
void vital_signs_replay_coarse(int best_slot) {
    for (int k = 0; k < COARSE_N; k++) {
        float v          = coarse_buf[best_slot][k];
        float filtered_b = apply_butterworth_b(v);
        float filtered_h = apply_butterworth_h(v);
        distance_history_b[dist_idx] = filtered_b;
        distance_history_h[dist_idx] = filtered_h;
        dist_idx++;
        if (dist_idx >= FFT_N) { dist_idx = 0; buffer_full = true; }
    }
}

// -----------------------------------------------------------------------
void vital_signs_init(void) {
    dist_idx = 0;
    buffer_full = false;
    slide_counter = 0;
    memset(filter_states_b, 0, sizeof(filter_states_b));
    memset(filter_states_h, 0, sizeof(filter_states_h));
    memset(distance_history_b, 0, sizeof(distance_history_b));
    memset(distance_history_h, 0, sizeof(distance_history_h));

    acc_algorithm_butter_bandpass(0.1f, 0.5f, SAMPLE_RATE_HZ, b_coeffs_b, a_coeffs_b);
    acc_algorithm_butter_bandpass(0.9f, 3.0f, SAMPLE_RATE_HZ, b_coeffs_h, a_coeffs_h);
    acc_algorithm_hamming(FFT_N, s_window);
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

    for (int k = 0; k < FFT_N; k++) {
      s_fft_b[k].real = distance_history_b[(dist_idx + k) % FFT_N] * s_window[k];
      s_fft_b[k].imag = 0.0f;
      s_fft_h[k].real = distance_history_h[(dist_idx + k) % FFT_N] * s_window[k];
      s_fft_h[k].imag = 0.0f;
    }

    compute_fft(s_fft_b, FFT_N);
    compute_fft(s_fft_h, FFT_N);

    for (int k = 0; k <= FFT_N/2; k++) {
      s_psd_b[k] = s_fft_b[k].real * s_fft_b[k].real + s_fft_b[k].imag * s_fft_b[k].imag;
      s_psd_h[k] = s_fft_h[k].real * s_fft_h[k].real + s_fft_h[k].imag * s_fft_h[k].imag;
    }

    // 搜索呼吸区间: 0.1Hz - 0.5Hz (6 - 30 BPM，排除低频躯体漂移)
    int b_min = (int)(0.1f * FFT_N / SAMPLE_RATE_HZ);
    int b_max = (int)(0.5f  * FFT_N / SAMPLE_RATE_HZ);
    float max_b = -1.0f;
    float avg_b = 0.0f;
    int peak_b_idx = b_min;
    for (int k = b_min; k <= b_max; k++) {
      avg_b += s_psd_b[k];
      if (s_psd_b[k] > max_b) { max_b = s_psd_b[k]; peak_b_idx = k; }
    }
    avg_b /= (b_max - b_min + 1);

    float freq_delta = SAMPLE_RATE_HZ / FFT_N;
    float freq_b = gaussian_peak_interp(s_psd_b, peak_b_idx, b_min, b_max, freq_delta);
    float snr_b = max_b / (avg_b + 1e-6f);
    bool b_ok = (snr_b > 3.0f);

    // 搜索心率区间: 0.9Hz - 3.0Hz (54 - 180 BPM)
    int h_min = (int)(0.9f * FFT_N / SAMPLE_RATE_HZ);
    int h_max = (int)(3.0f * FFT_N / SAMPLE_RATE_HZ);
    float max_h = -1.0f;
    int peak_h_idx = h_min;
    float avg_h = 0.0f;
    for (int k = h_min; k <= h_max; k++) {
      avg_h += s_psd_h[k];
      if (s_psd_h[k] > max_h) { max_h = s_psd_h[k]; peak_h_idx = k; }
    }
    avg_h /= (h_max - h_min + 1);

    float freq_h = gaussian_peak_interp(s_psd_h, peak_h_idx, h_min, h_max, freq_delta);
    
    // 心跳的 SNR 计算 (由于去除了呼吸频率的干扰，SNR 会更纯粹)
    float snr_h = max_h / (avg_h + 0.000001f);
    bool h_ok = (snr_h > 4.0f);

    float bpm_b = freq_b * 60.0f;
    float bpm_h = freq_h * 60.0f;

    printf("[Vitals] Dist: %" PRIfloat "m | Resp: ", ACC_LOG_FLOAT_TO_INTEGER(current_dist));
    if (b_ok) {
      printf("%" PRIfloat " BPM (SNR: %d)", ACC_LOG_FLOAT_TO_INTEGER(bpm_b), (int)snr_b);
    } else {
      printf("[Calc... Resp SNR: %d]", (int)snr_b);
    }

    printf(" | Heart: ");
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
