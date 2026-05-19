#ifndef VITAL_SIGNS_H_
#define VITAL_SIGNS_H_

#include "app_config.h"

#define COARSE_N         128   // 粗扫帧数 = 6.4s @20Hz，2的幂次，可直接 FFT
#define COARSE_MAX_CANDS 64    // 最多同时评估的候选 bin 数 (全频段扫描)

// --- 精测接口 ---
void vital_signs_init(void);
void process_vital_signs(float difference, float current_dist);

// --- 粗扫接口 ---
void vital_signs_coarse_start(void);
void vital_signs_coarse_feed(int slot, float angle);  // 每帧喂入每个候选 bin 的相位
bool vital_signs_coarse_tick(void);                   // 每帧调用一次，满 COARSE_N 帧返回 true
int  vital_signs_coarse_pick_best(int n_candidates);  // 返回最佳 slot 编号，无信号返回 -1
void vital_signs_replay_coarse(int best_slot);        // 把粗扫数据回放进精测滤波器，缩短等待时间

#endif
