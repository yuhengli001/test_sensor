// Copyright (c) 2026 haohanxu55-lang
// All rights reserved

#include "vital_signs.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "main.h"
#include "vital_sign_service_app.h"


#include "acc_algorithm.h"
#include "acc_definitions_a121.h"
#include "acc_integration_log.h"


typedef struct
{
	float real;
	float imag;
} complex_t;


#define WINDOW_LEN 128

static float    distance_history_b[WINDOW_LEN] = {0};
static float    distance_history_h[WINDOW_LEN] = {0};
static uint16_t dist_idx                        = 0;
static bool     buffer_full                     = false;
static uint16_t slide_counter                   = 0;

// Butterworth filter state — breathing band (0.15–0.7 Hz)
static float b_coeffs_b[5];
static float a_coeffs_b[4];
static float filter_states_b[5] = {0};

// Butterworth filter state — heartbeat band (0.9–3.0 Hz)
static float b_coeffs_h[5];
static float a_coeffs_h[4];
static float filter_states_h[5] = {0};
static bool  filter_initialized = false;

// BPM median filter: last 5 valid estimates, eliminates single-frame jumps
#define BPM_HIST_N 5
static float bpm_b_hist[BPM_HIST_N];
static float bpm_h_hist[BPM_HIST_N];
static int   bpm_b_hist_cnt;
static int   bpm_h_hist_cnt;
static int   bpm_b_hist_idx;
static int   bpm_h_hist_idx;

// Fine measurement FFT workspace
static complex_t s_fft_b[FFT_N];
static complex_t s_fft_h[FFT_N];
static float     s_psd_b_smooth[FFT_N / 2 + 1];
static float     s_psd_h_smooth[FFT_N / 2 + 1];
static float     s_window[FFT_N];

// Coarse sweep state (~2.5 KB additional static RAM)
static float     coarse_buf[COARSE_MAX_CANDS][COARSE_N];
static int       coarse_fill;
static bool      coarse_active;
static complex_t coarse_fft_work[COARSE_N]; // 128-point coarse FFT workspace (128 × 8 B = 1 KB)

// LMS adaptive noise canceller
#define LMS_ORDER 16
#define LMS_DELAY 10
static float lms_w[LMS_ORDER];
static float lms_x[LMS_ORDER + LMS_DELAY];
static float lms_mu = 0.002f;

// Heart-rate tracking window
static float    tracked_heart_freq  = 0.0f;
static uint32_t switch_frame_cnt    = 0;

// Breathing-rate tracking window
static float    tracked_resp_freq      = 0.0f;
static uint32_t switch_resp_frame_cnt  = 0;

// 0.05 Hz high-pass filter — removes DC offset and slow drift from the unwrapped phase
static float hp_x_prev = 0.0f;
static float hp_y_prev = 0.0f;
static bool  hp_first  = true;


static float apply_hp(float x)
{
	if (hp_first)
	{
		hp_x_prev = x;
		hp_y_prev = 0.0f;
		hp_first  = false;
		return 0.0f;
	}

	// alpha = 0.9845 → fc = 0.05 Hz at dt = 0.05 s
	float y   = 0.9845f * (hp_y_prev + x - hp_x_prev);
	hp_x_prev = x;
	hp_y_prev = y;
	return y;
}


// Insertion-sort median over up to BPM_HIST_N values (n<=5, negligible cost)
static float compute_median5(float *buf, int n)
{
	float tmp[BPM_HIST_N];

	for (int i = 0; i < n; i++)
	{
		tmp[i] = buf[i];
	}

	for (int i = 1; i < n; i++)
	{
		float v = tmp[i];
		int   j = i - 1;

		while (j >= 0 && tmp[j] > v)
		{
			tmp[j + 1] = tmp[j];
			j--;
		}

		tmp[j + 1] = v;
	}

	return tmp[n / 2];
}


// Gaussian interpolation for sub-bin peak localization.
// Fits a Gaussian in log-domain (equivalent to log-parabolic interpolation).
static float gaussian_peak_interp(const float *psd, int k, int k_min, int k_max, float freq_delta)
{
	if (k <= k_min || k >= k_max)
	{
		return k * freq_delta;
	}

	float a     = logf(psd[k - 1] + 1e-30f);
	float b     = logf(psd[k]     + 1e-30f);
	float c     = logf(psd[k + 1] + 1e-30f);
	float denom = a - 2.0f * b + c;

	if (fabsf(denom) < 1e-20f)
	{
		return k * freq_delta;
	}

	float delta = 0.5f * (a - c) / denom;

	// Valid range is [-0.5, 0.5]; outside means the peak is not centred here
	if (delta < -0.5f || delta > 0.5f)
	{
		return k * freq_delta;
	}

	return (k + delta) * freq_delta;
}


static void bit_reverse(complex_t *x, int n)
{
	int i, j, k;

	for (i = 1, j = 0; i < n - 1; i++)
	{
		for (k = n >> 1; (!((j ^= k) & k)); k >>= 1)
		{
			;
		}

		if (i < j)
		{
			complex_t temp = x[i];
			x[i]           = x[j];
			x[j]           = temp;
		}
	}
}


static void compute_fft(complex_t *x, int n)
{
	bit_reverse(x, n);

	for (int step = 1; step < n; step <<= 1)
	{
		float theta      = -3.14159265f / step;
		float w_real_step = cosf(theta);
		float w_imag_step = sinf(theta);

		for (int i = 0; i < n; i += 2 * step)
		{
			float w_real = 1.0f;
			float w_imag = 0.0f;

			for (int j = 0; j < step; j++)
			{
				int   a      = i + j;
				int   b      = i + j + step;
				float t_real = w_real * x[b].real - w_imag * x[b].imag;
				float t_imag = w_real * x[b].imag + w_imag * x[b].real;

				x[b].real = x[a].real - t_real;
				x[b].imag = x[a].imag - t_imag;
				x[a].real += t_real;
				x[a].imag += t_imag;

				float w_real_next = w_real * w_real_step - w_imag * w_imag_step;
				float w_imag_next = w_real * w_imag_step + w_imag * w_real_step;
				w_real            = w_real_next;
				w_imag            = w_imag_next;
			}
		}
	}
}


static float apply_butterworth_b(float input)
{
	float output     = b_coeffs_b[0] * input + filter_states_b[0];
	filter_states_b[0] = b_coeffs_b[1] * input - a_coeffs_b[0] * output + filter_states_b[1];
	filter_states_b[1] = b_coeffs_b[2] * input - a_coeffs_b[1] * output + filter_states_b[2];
	filter_states_b[2] = b_coeffs_b[3] * input - a_coeffs_b[2] * output + filter_states_b[3];
	filter_states_b[3] = b_coeffs_b[4] * input - a_coeffs_b[3] * output;
	return output;
}


static float apply_butterworth_h(float input)
{
	float output     = b_coeffs_h[0] * input + filter_states_h[0];
	filter_states_h[0] = b_coeffs_h[1] * input - a_coeffs_h[0] * output + filter_states_h[1];
	filter_states_h[1] = b_coeffs_h[2] * input - a_coeffs_h[1] * output + filter_states_h[2];
	filter_states_h[2] = b_coeffs_h[3] * input - a_coeffs_h[2] * output + filter_states_h[3];
	filter_states_h[3] = b_coeffs_h[4] * input - a_coeffs_h[3] * output;
	return output;
}


// Compute FFT of a single coarse-sweep slot and return the peak SNR in [f_lo, f_hi] Hz
static float compute_coarse_snr(int slot, float f_lo, float f_hi)
{
	// Remove DC component to prevent low-frequency leakage
	float mean = 0.0f;

	for (int k = 0; k < COARSE_N; k++)
	{
		mean += coarse_buf[slot][k];
	}

	mean /= COARSE_N;

	for (int k = 0; k < COARSE_N; k++)
	{
		coarse_fft_work[k].real = coarse_buf[slot][k] - mean;
		coarse_fft_work[k].imag = 0.0f;
	}

	compute_fft(coarse_fft_work, COARSE_N);

	float freq_res = SAMPLE_RATE_HZ / COARSE_N;
	int   k_lo     = (int)(f_lo / freq_res);
	int   k_hi     = (int)(f_hi / freq_res);

	if (k_hi >= COARSE_N / 2)
	{
		k_hi = COARSE_N / 2 - 1;
	}

	float max_p = 0.0f;
	float sum_p = 0.0f;
	int   cnt   = k_hi - k_lo + 1;

	for (int k = k_lo; k <= k_hi; k++)
	{
		float p = coarse_fft_work[k].real * coarse_fft_work[k].real
		        + coarse_fft_work[k].imag * coarse_fft_work[k].imag;
		sum_p += p;

		if (p > max_p)
		{
			max_p = p;
		}
	}

	float avg_p = (cnt > 0) ? sum_p / cnt : 0.0f;
	return max_p / (avg_p + 1e-30f);
}


/* -------------------------------------------------------------------------- */
/* Coarse sweep public interface                                               */
/* -------------------------------------------------------------------------- */

void vital_signs_coarse_start(void)
{
	memset(coarse_buf, 0, sizeof(coarse_buf));
	coarse_fill   = 0;
	coarse_active = true;
}


void vital_signs_coarse_feed(int slot, float angle)
{
	if (!coarse_active || slot < 0 || slot >= COARSE_MAX_CANDS)
	{
		return;
	}

	if (coarse_fill < COARSE_N)
	{
		coarse_buf[slot][coarse_fill] = angle;
	}
}


bool vital_signs_coarse_tick(void)
{
	if (!coarse_active)
	{
		return false;
	}

	coarse_fill++;
	return (coarse_fill >= COARSE_N);
}


int vital_signs_coarse_pick_best(int n_candidates)
{
	coarse_active = false;

	if (n_candidates > COARSE_MAX_CANDS)
	{
		n_candidates = COARSE_MAX_CANDS;
	}

	int closest_slot = -1;

	for (int s = 0; s < n_candidates; s++)
	{
		float r_snr = compute_coarse_snr(s, 0.15f, 0.7f);
		float h_snr = compute_coarse_snr(s, 0.9f,  3.0f);

		// Only log bins that show some signal to avoid flooding the console
		if (h_snr > 1.2f || r_snr > 1.2f)
		{
			printf("[Coarse] Bin %d: resp SNR=%.1f  heart SNR=%.1f\n", s, r_snr, h_snr);
		}

		// Accept a bin if the heartbeat SNR is above the minimum threshold
		if (h_snr > 1.5f && closest_slot == -1)
		{
			closest_slot = s; // Take the closest valid bin (bins are ordered near-to-far)
		}
	}

	if (closest_slot != -1)
	{
		printf("[Coarse] Selected closest valid bin: %d\n", closest_slot);
		return closest_slot;
	}

	return -1;
}


void vital_signs_replay_coarse(int best_slot)
{
	for (int k = 0; k < COARSE_N; k++)
	{
		float v          = coarse_buf[best_slot][k];
		float hp_v       = apply_hp(v);
		float filtered_b = apply_butterworth_b(hp_v);
		float filtered_h = apply_butterworth_h(hp_v);

		distance_history_b[dist_idx] = filtered_b;
		distance_history_h[dist_idx] = filtered_h;
		dist_idx++;

		if (dist_idx >= WINDOW_LEN)
		{
			dist_idx    = 0;
			buffer_full = true;
		}
	}

	// Force the FFT to trigger on the very next frame
	slide_counter = 10;
}


/* -------------------------------------------------------------------------- */
/* Fine measurement public interface                                           */
/* -------------------------------------------------------------------------- */

void vital_signs_init(void)
{
	dist_idx    = 0;
	buffer_full = false;
	slide_counter = 0;

	memset(filter_states_b,    0, sizeof(filter_states_b));
	memset(filter_states_h,    0, sizeof(filter_states_h));
	memset(distance_history_b, 0, sizeof(distance_history_b));
	memset(distance_history_h, 0, sizeof(distance_history_h));
	memset(s_psd_b_smooth,     0, sizeof(s_psd_b_smooth));
	memset(s_psd_h_smooth,     0, sizeof(s_psd_h_smooth));

	acc_algorithm_butter_bandpass(0.15f, 0.7f, SAMPLE_RATE_HZ, b_coeffs_b, a_coeffs_b);
	acc_algorithm_butter_bandpass(0.9f,  3.0f, SAMPLE_RATE_HZ, b_coeffs_h, a_coeffs_h);
	acc_algorithm_hamming(FFT_N, s_window);

	memset(bpm_b_hist, 0, sizeof(bpm_b_hist));
	memset(bpm_h_hist, 0, sizeof(bpm_h_hist));
	bpm_b_hist_cnt = 0;
	bpm_b_hist_idx = 0;
	bpm_h_hist_cnt = 0;
	bpm_h_hist_idx = 0;

	memset(lms_w, 0, sizeof(lms_w));
	memset(lms_x, 0, sizeof(lms_x));

	tracked_heart_freq    = 0.0f;
	switch_frame_cnt      = 0;
	tracked_resp_freq     = 0.0f;
	switch_resp_frame_cnt = 0;

	hp_first = true;

	filter_initialized = true;
}


void process_vital_signs(float difference, float current_dist)
{
	if (!global_config.enable_vitals_monitoring)
	{
		return;
	}

	if (!filter_initialized)
	{
		vital_signs_init();
	}

	// 1. High-pass filter (0.05 Hz) to remove DC offset and slow phase drift
	float hp_diff = apply_hp(difference);

	// 2. Breathing band-pass filter
	float filtered_b = apply_butterworth_b(hp_diff);

	// 3. LMS adaptive noise canceller — remove periodic breathing from the heart signal
	//    Shift the reference delay line
	for (int i = LMS_ORDER + LMS_DELAY - 1; i > 0; i--)
	{
		lms_x[i] = lms_x[i - 1];
	}

	lms_x[0] = hp_diff;

	// Estimate the periodic breathing component
	float lms_y = 0.0f;

	for (int i = 0; i < LMS_ORDER; i++)
	{
		lms_y += lms_w[i] * lms_x[i + LMS_DELAY];
	}

	// Cancel the breathing estimate from the input signal
	float raw_clean = hp_diff - lms_y;

	// Compute reference input energy for normalised step size
	float lms_energy = 1e-3f;

	for (int i = 0; i < LMS_ORDER; i++)
	{
		float val = lms_x[i + LMS_DELAY];
		lms_energy += val * val;
	}

	float norm_step = lms_mu / lms_energy;

	if (norm_step > 0.05f)
	{
		norm_step = 0.05f; // Hard limit to guarantee convergence during transients
	}

	// Update LMS weights with Leaky NLMS to prevent long-term drift
	for (int i = 0; i < LMS_ORDER; i++)
	{
		lms_w[i] = 0.999f * lms_w[i] + 2.0f * norm_step * raw_clean * lms_x[i + LMS_DELAY];
	}

	// 4. Heart-rate band-pass filter applied to the noise-cancelled signal
	float clean_h = apply_butterworth_h(raw_clean);

	// 5. Store into history buffers
	distance_history_b[dist_idx] = filtered_b;
	distance_history_h[dist_idx] = clean_h;
	dist_idx++;

	if (dist_idx >= WINDOW_LEN)
	{
		dist_idx    = 0;
		buffer_full = true;
	}

	// 6. Sliding-window FFT (runs every 10 new samples once the buffer is full)
	slide_counter++;

	if (!buffer_full || slide_counter < 10)
	{
		if (!buffer_full && (dist_idx % 40 == 0))
		{
			printf("[Vitals] Buffering: %d/%d\n", dist_idx, FFT_N);
		}

		return;
	}

	slide_counter = 0;
	int valid_len = WINDOW_LEN;

	for (int k = 0; k < FFT_N; k++)
	{
		if (k < valid_len)
		{
			float w   = 0.54f - 0.46f * cosf(2.0f * 3.14159265f * k / (valid_len - 1));
			int   idx = (dist_idx + k) % WINDOW_LEN;

			s_fft_b[k].real = distance_history_b[idx] * w;
			s_fft_b[k].imag = 0.0f;
			s_fft_h[k].real = distance_history_h[idx] * w;
			s_fft_h[k].imag = 0.0f;
		}
		else
		{
			// Zero-padding to FFT_N improves frequency resolution
			s_fft_b[k].real = 0.0f;
			s_fft_b[k].imag = 0.0f;
			s_fft_h[k].real = 0.0f;
			s_fft_h[k].imag = 0.0f;
		}
	}

	compute_fft(s_fft_b, FFT_N);
	compute_fft(s_fft_h, FFT_N);

	// Update smoothed PSD with first-order IIR (alpha=0.5, ~1 s time constant)
	for (int k = 0; k <= FFT_N / 2; k++)
	{
		float psd_b = s_fft_b[k].real * s_fft_b[k].real + s_fft_b[k].imag * s_fft_b[k].imag;
		float psd_h = s_fft_h[k].real * s_fft_h[k].real + s_fft_h[k].imag * s_fft_h[k].imag;

		if (s_psd_b_smooth[k] == 0.0f)
		{
			s_psd_b_smooth[k] = psd_b;
			s_psd_h_smooth[k] = psd_h;
		}
		else
		{
			s_psd_b_smooth[k] = 0.5f * s_psd_b_smooth[k] + 0.5f * psd_b;
			s_psd_h_smooth[k] = 0.5f * s_psd_h_smooth[k] + 0.5f * psd_h;
		}
	}

	// --- Breathing rate estimation (0.15–0.7 Hz, 9–42 BPM) ---
	int   b_min      = (int)(0.15f * FFT_N / SAMPLE_RATE_HZ);
	int   b_max      = (int)(0.7f  * FFT_N / SAMPLE_RATE_HZ);
	float freq_delta = SAMPLE_RATE_HZ / FFT_N;

	if (b_min < 4)
	{
		b_min = 4; // Exclude DC and sub-9 BPM slow drift
	}

	#define MAX_RESP_CANDIDATES 4

	typedef struct
	{
		float freq;
		float power;
		float score;
	} resp_cand_t;

	resp_cand_t resp_cands[MAX_RESP_CANDIDATES];
	int         resp_cand_cnt = 0;
	float       avg_b         = 0.0f;

	for (int k = b_min; k <= b_max; k++)
	{
		avg_b += s_psd_b_smooth[k];
	}

	avg_b /= (b_max - b_min + 1);

	// Find local maxima as breathing candidates
	for (int k = b_min; k <= b_max && resp_cand_cnt < MAX_RESP_CANDIDATES; k++)
	{
		bool is_local_max = true;

		if (k > 0         && s_psd_b_smooth[k] < s_psd_b_smooth[k - 1])
		{
			is_local_max = false;
		}

		if (k < FFT_N / 2 && s_psd_b_smooth[k] < s_psd_b_smooth[k + 1])
		{
			is_local_max = false;
		}

		if (is_local_max)
		{
			float f = gaussian_peak_interp(s_psd_b_smooth, k, b_min, b_max, freq_delta);
			resp_cands[resp_cand_cnt].freq  = f;
			resp_cands[resp_cand_cnt].power = s_psd_b_smooth[k];
			resp_cands[resp_cand_cnt].score = 0.0f;
			resp_cand_cnt++;
		}
	}

	// Fallback: use the strongest bin if no local maxima were found
	if (resp_cand_cnt == 0)
	{
		float max_val = -1.0f;
		int   best_k  = b_min + 1;

		if (best_k > b_max)
		{
			best_k = b_max;
		}

		for (int k = b_min + 1; k <= b_max; k++)
		{
			if (s_psd_b_smooth[k] > max_val)
			{
				max_val = s_psd_b_smooth[k];
				best_k  = k;
			}
		}

		if (max_val < 0.0f)
		{
			max_val = s_psd_b_smooth[b_min];
			best_k  = b_min;
		}

		resp_cands[0].freq  = gaussian_peak_interp(s_psd_b_smooth, best_k, b_min, b_max, freq_delta);
		resp_cands[0].power = max_val;
		resp_cands[0].score = max_val;
		resp_cand_cnt       = 1;
	}

	// Score candidates; boost frequency near the tracked breathing rate
	float best_resp_score = -1.0f;
	float freq_b_win      = resp_cands[0].freq;
	float power_b_win     = resp_cands[0].power;

	for (int i = 0; i < resp_cand_cnt; i++)
	{
		float freq  = resp_cands[i].freq;
		float power = resp_cands[i].power;
		float boost = 1.0f;

		if (tracked_resp_freq > 0.0f && fabsf(freq - tracked_resp_freq) < 0.08f)
		{
			boost = 2.0f; // Within 4.8 BPM of tracked frequency
		}

		float score         = power * boost;
		resp_cands[i].score = score;

		if (score > best_resp_score)
		{
			best_resp_score = score;
			freq_b_win      = freq;
			power_b_win     = power;
		}
	}

	// Update breathing tracking state machine
	if (tracked_resp_freq == 0.0f)
	{
		tracked_resp_freq     = freq_b_win;
		switch_resp_frame_cnt = 0;
	}
	else if (fabsf(freq_b_win - tracked_resp_freq) < 0.08f)
	{
		// Locked: smooth update
		tracked_resp_freq     = 0.9f * tracked_resp_freq + 0.1f * freq_b_win;
		switch_resp_frame_cnt = 0;
	}
	else
	{
		// Diverged: count frames and force re-lock after 6 consecutive updates (~3 s)
		switch_resp_frame_cnt++;

		if (switch_resp_frame_cnt >= 6)
		{
			tracked_resp_freq     = freq_b_win;
			switch_resp_frame_cnt = 0;
		}
	}

	float freq_b = tracked_resp_freq;
	float max_b  = power_b_win;
	float snr_b  = max_b / (avg_b + 1e-6f);
	bool  b_ok   = (snr_b > 2.0f);

	// --- Heart-rate estimation (0.9–3.0 Hz, 54–180 BPM) ---

	// Suppress breathing harmonics (3rd–6th order) from the heart PSD
	if (b_ok)
	{
		for (int m = 3; m <= 6; m++)
		{
			float f_h  = m * freq_b;
			int   h_idx = (int)(f_h / freq_delta + 0.5f);

			// Suppress the harmonic bin and its two neighbours
			for (int offset = -1; offset <= 1; offset++)
			{
				int   h_min = (int)(0.9f * FFT_N / SAMPLE_RATE_HZ);
				int   h_max = (int)(3.0f * FFT_N / SAMPLE_RATE_HZ);
				int   idx   = h_idx + offset;

				if (idx >= h_min && idx <= h_max)
				{
					s_psd_h_smooth[idx] *= 0.15f;
				}
			}
		}
	}

	int h_min = (int)(0.9f * FFT_N / SAMPLE_RATE_HZ);
	int h_max = (int)(3.0f * FFT_N / SAMPLE_RATE_HZ);

	#define MAX_CANDIDATE_PEAKS 4

	typedef struct
	{
		float freq;
		float power;
		float score;
	} cand_peak_t;

	cand_peak_t candidates[MAX_CANDIDATE_PEAKS];
	int         cand_cnt = 0;
	float       avg_h    = 0.0f;

	for (int k = h_min; k <= h_max; k++)
	{
		avg_h += s_psd_h_smooth[k];
	}

	avg_h /= (h_max - h_min + 1);

	// Find local maxima as heart-rate candidates
	for (int k = h_min + 1; k < h_max && cand_cnt < MAX_CANDIDATE_PEAKS; k++)
	{
		if (s_psd_h_smooth[k] > s_psd_h_smooth[k - 1] &&
		    s_psd_h_smooth[k] > s_psd_h_smooth[k + 1])
		{
			float f = gaussian_peak_interp(s_psd_h_smooth, k, h_min, h_max, freq_delta);
			candidates[cand_cnt].freq  = f;
			candidates[cand_cnt].power = s_psd_h_smooth[k];
			candidates[cand_cnt].score = 0.0f;
			cand_cnt++;
		}
	}

	// Fallback: use the global maximum
	if (cand_cnt == 0)
	{
		float max_val = -1.0f;
		int   best_k  = h_min;

		for (int k = h_min; k <= h_max; k++)
		{
			if (s_psd_h_smooth[k] > max_val)
			{
				max_val = s_psd_h_smooth[k];
				best_k  = k;
			}
		}

		candidates[0].freq  = gaussian_peak_interp(s_psd_h_smooth, best_k, h_min, h_max, freq_delta);
		candidates[0].power = max_val;
		candidates[0].score = max_val;
		cand_cnt            = 1;
	}

	// Score heart-rate candidates: penalise breathing harmonics, boost tracked frequency
	float best_score  = -1.0f;
	float freq_h_win  = candidates[0].freq;
	float power_h_win = candidates[0].power;

	for (int i = 0; i < cand_cnt; i++)
	{
		float freq    = candidates[i].freq;
		float power   = candidates[i].power;
		float penalty = 1.0f;
		float boost   = 1.0f;

		// Penalise breathing harmonics (3rd–6th order)
		if (b_ok)
		{
			for (int m = 3; m <= 6; m++)
			{
				if (fabsf(freq - m * freq_b) < 0.08f)
				{
					penalty = 0.1f;
					break;
				}
			}
		}

		// Boost frequencies near the currently tracked heart rate
		if (tracked_heart_freq > 0.0f && fabsf(freq - tracked_heart_freq) < 0.15f)
		{
			boost = 2.0f; // Within 9 BPM of tracked frequency
		}

		float score        = power * penalty * boost;
		candidates[i].score = score;

		if (score > best_score)
		{
			best_score  = score;
			freq_h_win  = freq;
			power_h_win = power;
		}
	}

	// Update heart-rate tracking state machine
	if (tracked_heart_freq == 0.0f)
	{
		tracked_heart_freq = freq_h_win;
		switch_frame_cnt   = 0;
	}
	else if (fabsf(freq_h_win - tracked_heart_freq) < 0.15f)
	{
		// Locked: smooth update
		tracked_heart_freq = 0.9f * tracked_heart_freq + 0.1f * freq_h_win;
		switch_frame_cnt   = 0;
	}
	else
	{
		// Diverged: force re-lock after 6 consecutive updates (~3 s)
		switch_frame_cnt++;

		if (switch_frame_cnt >= 6)
		{
			tracked_heart_freq = freq_h_win;
			switch_frame_cnt   = 0;
		}
	}

	float freq_h = tracked_heart_freq;
	float snr_h  = power_h_win / (avg_h + 0.000001f);
	float bpm_b  = freq_b * 60.0f;
	float bpm_h  = freq_h * 60.0f;

	// Update BPM median filter history
	bpm_b_hist[bpm_b_hist_idx] = bpm_b;
	bpm_b_hist_idx = (bpm_b_hist_idx + 1) % BPM_HIST_N;

	if (bpm_b_hist_cnt < BPM_HIST_N)
	{
		bpm_b_hist_cnt++;
	}

	bpm_h_hist[bpm_h_hist_idx] = bpm_h;
	bpm_h_hist_idx = (bpm_h_hist_idx + 1) % BPM_HIST_N;

	if (bpm_h_hist_cnt < BPM_HIST_N)
	{
		bpm_h_hist_cnt++;
	}

	float smooth_b = compute_median5(bpm_b_hist, bpm_b_hist_cnt);
	float smooth_h = compute_median5(bpm_h_hist, bpm_h_hist_cnt);

	bool h_ok = (snr_h > 2.0f);
	VITAL_APP_UpdateData(smooth_b, h_ok ? smooth_h : 0.0f, current_dist);

	printf("[Vitals] Dist: %" PRIfloat "m | Resp: %" PRIfloat " BPM (SNR: %d) | Heart: %" PRIfloat " BPM (SNR: %d)\n",
	       ACC_LOG_FLOAT_TO_INTEGER(current_dist),
	       ACC_LOG_FLOAT_TO_INTEGER(smooth_b), (int)snr_b,
	       ACC_LOG_FLOAT_TO_INTEGER(smooth_h), (int)snr_h);
}
