#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "key_recovery_cuda_backend.h"

static const uint8_t PREP_PHASE_TABLE[256] = {
    216,  25,  88, 120,  57,  89, 184, 153,  56, 217, 152, 248, 121, 185,  24, 249,
    200,   9,  72, 104,  41,  73, 168, 137,  40, 201, 136, 232, 105, 169,   8, 233,
    204,  13,  76, 108,  45,  77, 172, 141,  44, 205, 140, 236, 109, 173,  12, 237,
    196,   5,  68, 100,  37,  69, 164, 133,  36, 197, 132, 228, 101, 165,   4, 229,
    212,  21,  84, 116,  53,  85, 180, 149,  52, 213, 148, 244, 117, 181,  20, 245,
    202,  11,  74, 106,  43,  75, 170, 139,  42, 203, 138, 234, 107, 171,  10, 235,
    214,  23,  86, 118,  55,  87, 182, 151,  54, 215, 150, 246, 119, 183,  22, 247,
    210,  19,  82, 114,  51,  83, 178, 147,  50, 211, 146, 242, 115, 179,  18, 243,
    220,  29,  92, 124,  61,  93, 188, 157,  60, 221, 156, 252, 125, 189,  28, 253,
    208,  17,  80, 112,  49,  81, 176, 145,  48, 209, 144, 240, 113, 177,  16, 241,
    218,  27,  90, 122,  59,  91, 186, 155,  58, 219, 154, 250, 123, 187,  26, 251,
    206,  15,  78, 110,  47,  79, 174, 143,  46, 207, 142, 238, 111, 175,  14, 239,
    192,   1,  64,  96,  33,  65, 160, 129,  32, 193, 128, 224,  97, 161,   0, 225,
    198,   7,  70, 102,  39,  71, 166, 135,  38, 199, 134, 230, 103, 167,   6, 231,
    222,  31,  94, 126,  63,  95, 190, 159,  62, 223, 158, 254, 127, 191,  30, 255,
    194,   3,  66,  98,  35,  67, 162, 131,  34, 195, 130, 226,  99, 163,   2, 227
};

static const uint8_t FULL_SCHEDULE[12] = {3, 2, 1, 0, 3, 2, 1, 0, 0, 1, 2, 3};
static const uint8_t PREFIX10_SCHEDULE[10] = {3, 2, 1, 0, 3, 2, 1, 0, 0, 1};

typedef struct {
    uint32_t key;
    uint16_t alpha;
    uint16_t beta;
    float delta_prefix;
    int32_t delta_prefix_signed_coeff;
    int32_t delta_prefix_abs_coeff;
    uint32_t delta_prefix_max_pair_count_tol_2_neg_15;
    int32_t delta_prefix_signed_coeff_min_tol_2_neg_15;
    int32_t delta_prefix_signed_coeff_max_tol_2_neg_15;
    uint32_t sample_count;
    uint8_t true_k1;
    uint8_t true_k2;
    int32_t true_score;
    double true_bias;
    int64_t false_score_sum;
    double false_bias_mean;
    uint32_t true_rank;
    uint8_t best_k1;
    uint8_t best_k2;
    int32_t best_score;
    double best_bias;
    uint32_t best_tie_count;
    uint32_t top_hits;
    int32_t max_abs_score_signed_min;
    int32_t max_abs_score_signed_max;
    int32_t min_abs_score_value;
    double delta_time_sec;
    double recovery_time_sec;
    double total_time_sec;
} IterationResult;

typedef struct {
    uint16_t index;
    int32_t score;
} CandidateScore;

typedef struct {
    uint32_t iterations;
    bool use_fixed_key;
    uint32_t fixed_key;
    uint64_t seed;
    double sample_factor_m;
    uint32_t sample_cap;
    uint32_t top_count;
    const char *output_dir;
    const char *series_label;
    bool save_full_candidates;
    bool resume;
    bool full_material;
    uint32_t thread_count;
    const char *reuse_prefix_summary;
    KeyRecoveryBackend backend_mode;
    uint32_t cuda_threshold_count;
} Config;

typedef struct {
    uint32_t iteration;
    uint32_t key;
    PrefixSpectrum spectrum;
    double delta_time_sec;
} PrefixCacheEntry;

static uint8_t ROUND_TABLE[256][256];
static uint8_t PARITY8_TABLE[256];
static uint16_t SAMPLE_DOMAIN[65536];
static const int32_t DELTA_TOLERANCE_COEFF_2_NEG_15 = 1;

typedef struct {
    const uint16_t *lookup_table;
    int beta_start;
    int beta_end;
    int *f_buffer;
    PrefixSpectrum result;
    int best_abs;
    uint32_t exact_pair_count;
    uint32_t near_pair_count;
    int32_t exact_signed_min;
    int32_t exact_signed_max;
    int32_t near_signed_min;
    int32_t near_signed_max;
} SpectrumWorker;

static inline double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static inline uint8_t parity16(uint16_t x) {
    return (uint8_t)__builtin_parity((unsigned)x);
}

static const char *path_basename_const(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

static const char *sample_mode_name(const Config *config) {
    return config->full_material ? "full_domain" : "adaptive_random_without_replacement";
}

static const char *backend_mode_name(KeyRecoveryBackend backend) {
    switch (backend) {
        case KEY_RECOVERY_BACKEND_CPU:
            return "cpu";
        case KEY_RECOVERY_BACKEND_CUDA:
            return "cuda";
        case KEY_RECOVERY_BACKEND_AUTO:
        default:
            return "auto";
    }
}

static void write_iteration_progress(
    const Config *config,
    uint32_t iteration_index,
    const IterationResult *result
) {
    char progress_path[1024];
    char progress_log_path[1024];
    char timestamp_buf[64];
    time_t now_time = time(NULL);
    struct tm tm_value;
    FILE *progress_file = NULL;
    FILE *progress_log = NULL;
    double percent = 0.0;

    localtime_r(&now_time, &tm_value);
    strftime(timestamp_buf, sizeof(timestamp_buf), "%Y-%m-%d %H:%M:%S", &tm_value);

    snprintf(progress_path, sizeof(progress_path), "%s/progress.txt", config->output_dir);
    snprintf(progress_log_path, sizeof(progress_log_path), "%s/progress.log", config->output_dir);

    percent = (100.0 * (double)(iteration_index + 1)) / (double)config->iterations;

    progress_file = fopen(progress_path, "w");
    if (progress_file) {
        fprintf(progress_file, "updated_at=%s\n", timestamp_buf);
        fprintf(progress_file, "series_label=%s\n", config->series_label ? config->series_label : "default");
        fprintf(progress_file, "completed_iterations=%" PRIu32 "\n", iteration_index + 1);
        fprintf(progress_file, "total_iterations=%" PRIu32 "\n", config->iterations);
        fprintf(progress_file, "progress_percent=%.4f\n", percent);
        fprintf(progress_file, "last_iteration=%" PRIu32 "\n", iteration_index + 1);
        fprintf(progress_file, "last_key=0x%08" PRIX32 "\n", result->key);
        fprintf(progress_file, "last_true_rank=%" PRIu32 "\n", result->true_rank);
        fprintf(progress_file, "last_sample_count=%" PRIu32 "\n", result->sample_count);
        fprintf(progress_file, "last_prefix_delta=%.10f\n", result->delta_prefix);
        fprintf(progress_file, "last_total_time_sec=%.4f\n", result->total_time_sec);
        fclose(progress_file);
    }

    progress_log = fopen(progress_log_path, "a");
    if (progress_log) {
        fprintf(
            progress_log,
            "%s completed=%" PRIu32 "/%" PRIu32 " (%.4f%%) key=0x%08" PRIX32 " rank=%" PRIu32 " samples=%" PRIu32 " total_time_sec=%.4f\n",
            timestamp_buf,
            iteration_index + 1,
            config->iterations,
            percent,
            result->key,
            result->true_rank,
            result->sample_count,
            result->total_time_sec
        );
        fclose(progress_log);
    }
}

static uint32_t power_of_two_exponent_u64(uint64_t value) {
    uint32_t exponent = 0;
    if (value == 0 || (value & (value - 1)) != 0) {
        return UINT32_MAX;
    }
    while (value > 1) {
        value >>= 1;
        exponent++;
    }
    return exponent;
}

static void format_fraction_i64(int64_t numerator, uint64_t denominator, char *buf, size_t buf_size) {
    uint32_t exponent = power_of_two_exponent_u64(denominator);
    if (exponent != UINT32_MAX) {
        snprintf(buf, buf_size, "%" PRId64 "/2^%" PRIu32, numerator, exponent);
    } else {
        snprintf(buf, buf_size, "%" PRId64 "/%" PRIu64, numerator, denominator);
    }
}

static inline uint8_t round_function(uint8_t x, uint8_t k) {
    return ROUND_TABLE[k][x];
}

static inline uint16_t swap16(uint16_t x) {
    return (uint16_t)((x << 8) | (x >> 8));
}

static inline void encrypt_round(uint16_t *block, uint8_t key_element) {
    uint8_t left = (uint8_t)((*block >> 8) & 0xFFu);
    uint8_t right = (uint8_t)(*block & 0xFFu);
    uint8_t new_left = (uint8_t)(right ^ round_function(left, key_element));
    *block = (uint16_t)(((uint16_t)new_left << 8) | left);
}

static uint16_t encrypt_block_full(uint16_t plaintext, const uint8_t key[4]) {
    uint16_t block = plaintext;
    for (size_t round = 0; round < 12; round++) {
        encrypt_round(&block, key[FULL_SCHEDULE[round]]);
    }
    return swap16(block);
}

static uint16_t encrypt_block_prefix10(uint16_t plaintext, const uint8_t key[4]) {
    uint16_t block = plaintext;
    for (size_t round = 0; round < 10; round++) {
        encrypt_round(&block, key[PREFIX10_SCHEDULE[round]]);
    }
    return block;
}

static void split_key_be(uint32_t raw_key, uint8_t key[4]) {
    key[0] = (uint8_t)((raw_key >> 24) & 0xFFu);
    key[1] = (uint8_t)((raw_key >> 16) & 0xFFu);
    key[2] = (uint8_t)((raw_key >> 8) & 0xFFu);
    key[3] = (uint8_t)(raw_key & 0xFFu);
}

static void init_tables(void) {
    for (int v = 0; v < 256; v++) {
        PARITY8_TABLE[v] = (uint8_t)__builtin_parity((unsigned)v);
    }

    for (int k = 0; k < 256; k++) {
        for (int x = 0; x < 256; x++) {
            ROUND_TABLE[k][x] = PREP_PHASE_TABLE[(x + k) & 0xFF];
        }
    }
}

static void fwt_int_65536(int *f) {
    for (int len = 1; len < 65536; len <<= 1) {
        for (int start = 0; start < 65536; start += (len << 1)) {
            for (int i = 0; i < len; i++) {
                int a = f[start + i];
                int b = f[start + i + len];
                f[start + i] = a + b;
                f[start + i + len] = a - b;
            }
        }
    }
}

static bool spectrum_is_better(
    int candidate_abs,
    uint16_t candidate_alpha,
    uint16_t candidate_beta,
    int current_abs,
    uint16_t current_alpha,
    uint16_t current_beta
) {
    if (candidate_abs > current_abs) {
        return true;
    }
    if (candidate_abs < current_abs) {
        return false;
    }
    if (current_abs < 0) {
        return true;
    }
    if (candidate_beta < current_beta) {
        return true;
    }
    if (candidate_beta > current_beta) {
        return false;
    }
    return candidate_alpha < current_alpha;
}

static void *compute_prefix_spectrum_worker(void *arg) {
    SpectrumWorker *worker = (SpectrumWorker *)arg;
    const float norm = 1.0f / 65536.0f;

    worker->best_abs = -1;
    worker->exact_pair_count = 0;
    worker->near_pair_count = 0;
    worker->exact_signed_min = INT32_MAX;
    worker->exact_signed_max = INT32_MIN;
    worker->near_signed_min = INT32_MAX;
    worker->near_signed_max = INT32_MIN;
    worker->result.alpha = 0;
    worker->result.beta = 0;
    worker->result.delta = 0.0f;
    worker->result.signed_correlation = 0;
    worker->result.abs_coeff = 0;
    worker->result.exact_pair_count = 0;
    worker->result.tol_pair_count = 0;
    worker->result.tol_signed_min = 0;
    worker->result.tol_signed_max = 0;

    for (int beta = worker->beta_start; beta < worker->beta_end; beta++) {
        for (int x = 0; x < 65536; x++) {
            uint16_t y = worker->lookup_table[x];
            worker->f_buffer[x] = parity16((uint16_t)(beta & y)) ? -1 : 1;
        }

        fwt_int_65536(worker->f_buffer);

        for (int alpha = 1; alpha < 65536; alpha++) {
            int coeff = worker->f_buffer[alpha];
            int abs_coeff = coeff < 0 ? -coeff : coeff;
            int32_t signed_coeff = -coeff;
            bool is_canonical_better = spectrum_is_better(
                abs_coeff,
                (uint16_t)alpha,
                (uint16_t)beta,
                worker->best_abs,
                worker->result.alpha,
                worker->result.beta
            );
            if (abs_coeff > worker->best_abs) {
                if (worker->best_abs == abs_coeff - DELTA_TOLERANCE_COEFF_2_NEG_15) {
                    worker->near_pair_count = worker->exact_pair_count;
                    worker->near_signed_min = worker->exact_signed_min;
                    worker->near_signed_max = worker->exact_signed_max;
                } else {
                    worker->near_pair_count = 0;
                    worker->near_signed_min = INT32_MAX;
                    worker->near_signed_max = INT32_MIN;
                }
                worker->best_abs = abs_coeff;
                worker->exact_pair_count = 1;
                worker->exact_signed_min = signed_coeff;
                worker->exact_signed_max = signed_coeff;
            } else if (abs_coeff == worker->best_abs) {
                worker->exact_pair_count++;
                if (signed_coeff < worker->exact_signed_min) {
                    worker->exact_signed_min = signed_coeff;
                }
                if (signed_coeff > worker->exact_signed_max) {
                    worker->exact_signed_max = signed_coeff;
                }
            } else if (abs_coeff == worker->best_abs - DELTA_TOLERANCE_COEFF_2_NEG_15) {
                worker->near_pair_count++;
                if (signed_coeff < worker->near_signed_min) {
                    worker->near_signed_min = signed_coeff;
                }
                if (signed_coeff > worker->near_signed_max) {
                    worker->near_signed_max = signed_coeff;
                }
            }

            if (is_canonical_better) {
                worker->result.alpha = (uint16_t)alpha;
                worker->result.beta = (uint16_t)beta;
                worker->result.delta = (float)abs_coeff * norm;
                worker->result.signed_correlation = signed_coeff;
            }
        }
    }

    worker->result.abs_coeff = worker->best_abs;
    worker->result.exact_pair_count = worker->exact_pair_count;
    worker->result.tol_pair_count = worker->exact_pair_count + worker->near_pair_count;
    if (worker->near_pair_count > 0) {
        worker->result.tol_signed_min = worker->exact_signed_min < worker->near_signed_min ? worker->exact_signed_min : worker->near_signed_min;
        worker->result.tol_signed_max = worker->exact_signed_max > worker->near_signed_max ? worker->exact_signed_max : worker->near_signed_max;
    } else {
        worker->result.tol_signed_min = worker->exact_signed_min == INT32_MAX ? 0 : worker->exact_signed_min;
        worker->result.tol_signed_max = worker->exact_signed_max == INT32_MIN ? 0 : worker->exact_signed_max;
    }

    return NULL;
}

static uint32_t default_thread_count(void) {
    long cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_count <= 0) {
        return 4;
    }
    if (cpu_count > 64) {
        return 64;
    }
    return (uint32_t)cpu_count;
}

static PrefixSpectrum compute_prefix_spectrum(const uint16_t *lookup_table, uint32_t thread_count) {
    PrefixSpectrum result = {0};
    SpectrumWorker *workers = NULL;
    pthread_t *threads = NULL;
    int global_best_abs = -1;
    uint32_t global_exact_pair_count = 0;
    uint32_t global_tol_pair_count = 0;
    int32_t global_tol_signed_min = INT32_MAX;
    int32_t global_tol_signed_max = INT32_MIN;
    const int beta_count = 65535;

    if (thread_count < 1) {
        thread_count = 1;
    }
    if (thread_count > (uint32_t)beta_count) {
        thread_count = (uint32_t)beta_count;
    }

    workers = (SpectrumWorker *)calloc(thread_count, sizeof(SpectrumWorker));
    threads = (pthread_t *)calloc(thread_count, sizeof(pthread_t));
    if (!workers || !threads) {
        fprintf(stderr, "Не удалось выделить память для многопоточного спектра\n");
        free(workers);
        free(threads);
        return result;
    }

    for (uint32_t t = 0; t < thread_count; t++) {
        int beta_start = 1 + (int)((uint64_t)t * (uint64_t)beta_count / thread_count);
        int beta_end = 1 + (int)((uint64_t)(t + 1) * (uint64_t)beta_count / thread_count);
        if (beta_end > 65536) {
            beta_end = 65536;
        }

        workers[t].lookup_table = lookup_table;
        workers[t].beta_start = beta_start;
        workers[t].beta_end = beta_end;
        workers[t].f_buffer = (int *)malloc(65536u * sizeof(int));
        if (!workers[t].f_buffer) {
            fprintf(stderr, "Не удалось выделить FWT буфер для потока %" PRIu32 "\n", t);
            thread_count = t;
            break;
        }
        if (pthread_create(&threads[t], NULL, compute_prefix_spectrum_worker, &workers[t]) != 0) {
            fprintf(stderr, "Не удалось создать поток %" PRIu32 "\n", t);
            free(workers[t].f_buffer);
            workers[t].f_buffer = NULL;
            thread_count = t;
            break;
        }
    }

    for (uint32_t t = 0; t < thread_count; t++) {
        pthread_join(threads[t], NULL);
        if (workers[t].best_abs > global_best_abs) {
            global_best_abs = workers[t].best_abs;
            result = workers[t].result;
            global_exact_pair_count = workers[t].exact_pair_count;
            global_tol_pair_count = workers[t].exact_pair_count + workers[t].near_pair_count;
            global_tol_signed_min = workers[t].exact_signed_min;
            global_tol_signed_max = workers[t].exact_signed_max;
            if (workers[t].near_pair_count > 0) {
                if (workers[t].near_signed_min < global_tol_signed_min) {
                    global_tol_signed_min = workers[t].near_signed_min;
                }
                if (workers[t].near_signed_max > global_tol_signed_max) {
                    global_tol_signed_max = workers[t].near_signed_max;
                }
            }
        } else {
            if (spectrum_is_better(
                workers[t].best_abs,
                workers[t].result.alpha,
                workers[t].result.beta,
                global_best_abs,
                result.alpha,
                result.beta
            )) {
                result = workers[t].result;
            }

            if (workers[t].best_abs == global_best_abs) {
                global_exact_pair_count += workers[t].exact_pair_count;
                global_tol_pair_count += workers[t].exact_pair_count + workers[t].near_pair_count;
                if (workers[t].exact_signed_min < global_tol_signed_min) {
                    global_tol_signed_min = workers[t].exact_signed_min;
                }
                if (workers[t].exact_signed_max > global_tol_signed_max) {
                    global_tol_signed_max = workers[t].exact_signed_max;
                }
                if (workers[t].near_pair_count > 0) {
                    if (workers[t].near_signed_min < global_tol_signed_min) {
                        global_tol_signed_min = workers[t].near_signed_min;
                    }
                    if (workers[t].near_signed_max > global_tol_signed_max) {
                        global_tol_signed_max = workers[t].near_signed_max;
                    }
                }
            } else if (workers[t].best_abs == global_best_abs - DELTA_TOLERANCE_COEFF_2_NEG_15) {
                global_tol_pair_count += workers[t].exact_pair_count;
                if (workers[t].exact_signed_min < global_tol_signed_min) {
                    global_tol_signed_min = workers[t].exact_signed_min;
                }
                if (workers[t].exact_signed_max > global_tol_signed_max) {
                    global_tol_signed_max = workers[t].exact_signed_max;
                }
            }
        }
        free(workers[t].f_buffer);
    }

    result.abs_coeff = global_best_abs < 0 ? 0 : global_best_abs;
    result.exact_pair_count = global_exact_pair_count;
    result.tol_pair_count = global_tol_pair_count;
    result.tol_signed_min = (global_tol_signed_min == INT32_MAX) ? 0 : global_tol_signed_min;
    result.tol_signed_max = (global_tol_signed_max == INT32_MIN) ? 0 : global_tol_signed_max;

    free(workers);
    free(threads);
    return result;
}

static int select_active_backend(
    const Config *config,
    KeyRecoveryBackend *active_backend,
    KeyRecoveryCudaInfo *cuda_info
) {
    if (cuda_info) {
        key_recovery_cuda_query(cuda_info);
    }

    if (config->reuse_prefix_summary) {
        *active_backend = KEY_RECOVERY_BACKEND_CPU;
        return 0;
    }

    if (config->backend_mode == KEY_RECOVERY_BACKEND_CPU) {
        *active_backend = KEY_RECOVERY_BACKEND_CPU;
        return 0;
    }

    if (config->backend_mode == KEY_RECOVERY_BACKEND_CUDA) {
        if (!cuda_info || !cuda_info->available) {
            fprintf(
                stderr,
                "Запрошен backend=cuda, но CUDA недоступна: %s\n",
                (cuda_info && cuda_info->status[0] != '\0') ? cuda_info->status : "unknown error"
            );
            return -1;
        }
        *active_backend = KEY_RECOVERY_BACKEND_CUDA;
        return 0;
    }

    if (cuda_info && cuda_info->available && config->iterations >= config->cuda_threshold_count) {
        *active_backend = KEY_RECOVERY_BACKEND_CUDA;
    } else {
        *active_backend = KEY_RECOVERY_BACKEND_CPU;
    }
    return 0;
}

static uint64_t splitmix64_next(uint64_t *state) {
    uint64_t z = (*state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

static uint64_t sample_rng_seed(uint64_t base_seed, uint32_t iteration_index, uint32_t key32) {
    uint64_t state = base_seed
        ^ ((uint64_t)iteration_index << 32)
        ^ (uint64_t)key32
        ^ 0xD1B54A32D192ED03ull;
    return splitmix64_next(&state);
}

static void fill_random_sample_without_replacement(
    uint16_t *sample_values,
    uint32_t sample_count,
    uint64_t seed
) {
    uint64_t state = seed;

    for (uint32_t i = 0; i < 65536u; i++) {
        SAMPLE_DOMAIN[i] = (uint16_t)i;
    }

    for (uint32_t i = 0; i < sample_count; i++) {
        uint32_t remaining = 65536u - i;
        uint32_t j = i + (uint32_t)(splitmix64_next(&state) % remaining);
        uint16_t tmp = SAMPLE_DOMAIN[i];
        SAMPLE_DOMAIN[i] = SAMPLE_DOMAIN[j];
        SAMPLE_DOMAIN[j] = tmp;
        sample_values[i] = SAMPLE_DOMAIN[i];
    }
}

static uint32_t sample_count_from_delta(double sample_factor_m, double delta, uint32_t sample_cap) {
    double denom = delta * delta;
    uint32_t sample_count = sample_cap;

    if (denom > 0.0) {
        double raw = ceil(sample_factor_m / denom);
        if (raw < 1.0) {
            raw = 1.0;
        }
        if (raw < (double)sample_cap) {
            sample_count = (uint32_t)raw;
        }
    }

    if (sample_count == 0) {
        sample_count = 1;
    }
    if (sample_count > 65536u) {
        sample_count = 65536u;
    }
    return sample_count;
}

static void update_top_candidates(CandidateScore *top, uint32_t top_count, uint16_t index, int32_t score) {
    int32_t abs_score = score < 0 ? -score : score;
    uint32_t insert_at = top_count;

    for (uint32_t i = 0; i < top_count; i++) {
        int32_t current_abs = 0;
        if (top[i].score == INT32_MIN) {
            insert_at = i;
            break;
        }
        current_abs = top[i].score < 0 ? -top[i].score : top[i].score;
        if (abs_score > current_abs) {
            insert_at = i;
            break;
        }
    }

    if (insert_at == top_count) {
        return;
    }

    for (uint32_t i = top_count - 1; i > insert_at; i--) {
        top[i] = top[i - 1];
    }
    top[insert_at].index = index;
    top[insert_at].score = score;
}

static int write_top_candidates(
    const Config *config,
    uint32_t iteration_index,
    uint32_t key,
    uint16_t alpha,
    uint16_t beta,
    const CandidateScore *top,
    uint32_t top_count,
    uint32_t sample_count
) {
    char path[1024];
    FILE *f = NULL;

    snprintf(
        path,
        sizeof(path),
        "%s/top_candidates_%04u_%08" PRIX32 ".csv",
        config->output_dir,
        iteration_index + 1,
        key
    );

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Не удалось открыть %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(
        f,
        "RUN_ID,SERIES_LABEL,SAMPLE_MODE,SAMPLE_FACTOR_M,SAMPLE_CAP,THREAD_COUNT,SEED,ITERATION,TRUE_KEY,PREFIX_ALPHA,PREFIX_BETA,RANK,K1_GUESS,K2_GUESS,SCORE_NUMERATOR,SCORE_DENOMINATOR,SCORE_FRACTION,SCORE_VALUE,ABS_SCORE_VALUE\n"
    );
    for (uint32_t i = 0; i < top_count; i++) {
        char score_fraction[64];
        uint8_t k1 = (uint8_t)(top[i].index >> 8);
        uint8_t k2 = (uint8_t)(top[i].index & 0xFFu);
        double bias = (double)top[i].score / (double)sample_count;
        double abs_bias = fabs(bias);
        format_fraction_i64((int64_t)top[i].score, (uint64_t)sample_count, score_fraction, sizeof(score_fraction));
        fprintf(
            f,
            "%s,%s,%s,%.6f,%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",%" PRIu32 ",0x%08" PRIX32 ",0x%04X,0x%04X,%" PRIu32 ",0x%02X,0x%02X,%" PRId32 ",%" PRIu32 ",%s,%.10f,%.10f\n",
            path_basename_const(config->output_dir),
            config->series_label ? config->series_label : "default",
            sample_mode_name(config),
            config->sample_factor_m,
            config->sample_cap,
            config->thread_count,
            config->seed,
            iteration_index + 1,
            key,
            alpha,
            beta,
            i + 1,
            k1,
            k2,
            top[i].score,
            sample_count,
            score_fraction,
            bias,
            abs_bias
        );
    }

    fclose(f);
    return 0;
}

static int write_full_candidates(
    const Config *config,
    uint32_t iteration_index,
    uint32_t key,
    uint16_t alpha,
    uint16_t beta,
    const int32_t *scores,
    uint32_t sample_count,
    uint8_t true_k1,
    uint8_t true_k2
) {
    char path[1024];
    FILE *f = NULL;

    snprintf(
        path,
        sizeof(path),
        "%s/all_candidates_%04u_%08" PRIX32 ".csv",
        config->output_dir,
        iteration_index + 1,
        key
    );

    f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "Не удалось открыть %s: %s\n", path, strerror(errno));
        return -1;
    }

    fprintf(
        f,
        "RUN_ID,SERIES_LABEL,SAMPLE_MODE,SAMPLE_FACTOR_M,SAMPLE_CAP,THREAD_COUNT,SEED,ITERATION,TRUE_KEY,PREFIX_ALPHA,PREFIX_BETA,K1_GUESS,K2_GUESS,SCORE_NUMERATOR,SCORE_DENOMINATOR,SCORE_FRACTION,SCORE_VALUE,ABS_SCORE_VALUE,IS_TRUE\n"
    );
    for (uint32_t index = 0; index < 65536u; index++) {
        char score_fraction[64];
        uint8_t k1 = (uint8_t)(index >> 8);
        uint8_t k2 = (uint8_t)(index & 0xFFu);
        double bias = (double)scores[index] / (double)sample_count;
        format_fraction_i64((int64_t)scores[index], (uint64_t)sample_count, score_fraction, sizeof(score_fraction));
        fprintf(
            f,
            "%s,%s,%s,%.6f,%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",%" PRIu32 ",0x%08" PRIX32 ",0x%04X,0x%04X,0x%02X,0x%02X,%" PRId32 ",%" PRIu32 ",%s,%.10f,%.10f,%s\n",
            path_basename_const(config->output_dir),
            config->series_label ? config->series_label : "default",
            sample_mode_name(config),
            config->sample_factor_m,
            config->sample_cap,
            config->thread_count,
            config->seed,
            iteration_index + 1,
            key,
            alpha,
            beta,
            k1,
            k2,
            scores[index],
            sample_count,
            score_fraction,
            bias,
            fabs(bias),
            (k1 == true_k1 && k2 == true_k2) ? "yes" : "no"
        );
    }

    fclose(f);
    return 0;
}

static int run_iteration(
    const Config *config,
    KeyRecoveryBackend active_backend,
    uint32_t iteration_index,
    uint32_t key32,
    FILE *summary_csv,
    uint16_t *prefix_lookup,
    uint16_t *sample_plaintexts,
    uint8_t *sample_plain_parity,
    uint8_t *sample_cipher_hi,
    uint8_t *sample_cipher_lo,
    int32_t *candidate_scores,
    const PrefixCacheEntry *cached_prefix
) {
    IterationResult result = {0};
    CandidateScore *top = NULL;
    uint8_t key[4];
    PrefixSpectrum spectrum = {0};
    uint8_t beta_left_parity[256];
    uint8_t beta_right_parity[256];
    int8_t sign_table[256][256];
    double total_start = now_sec();
    double delta_start = 0.0;
    double recovery_start = 0.0;

    result.key = key32;
    split_key_be(key32, key);
    result.true_k1 = key[3];
    result.true_k2 = key[2];

    if (cached_prefix) {
        spectrum = cached_prefix->spectrum;
        result.delta_time_sec = 0.0;
    } else if (active_backend == KEY_RECOVERY_BACKEND_CUDA) {
        char cuda_error[256] = {0};
        if (key_recovery_cuda_compute_prefix_spectrum(
            key32,
            &spectrum,
            &result.delta_time_sec,
            cuda_error,
            sizeof(cuda_error)
        ) != 0) {
            fprintf(stderr, "CUDA prefix spectrum failed for key 0x%08" PRIX32 ": %s\n", key32, cuda_error);
            return -1;
        }
    } else {
        delta_start = now_sec();
        for (uint32_t x = 0; x < 65536u; x++) {
            prefix_lookup[x] = encrypt_block_prefix10((uint16_t)x, key);
        }
        spectrum = compute_prefix_spectrum(prefix_lookup, config->thread_count);
        result.delta_time_sec = now_sec() - delta_start;
    }
    result.alpha = spectrum.alpha;
    result.beta = spectrum.beta;
    result.delta_prefix = spectrum.delta;
    result.delta_prefix_signed_coeff = spectrum.signed_correlation;
    result.delta_prefix_abs_coeff = spectrum.abs_coeff;
    result.delta_prefix_max_pair_count_tol_2_neg_15 = spectrum.tol_pair_count;
    result.delta_prefix_signed_coeff_min_tol_2_neg_15 = spectrum.tol_signed_min;
    result.delta_prefix_signed_coeff_max_tol_2_neg_15 = spectrum.tol_signed_max;
    result.sample_count = config->full_material
        ? 65536u
        : sample_count_from_delta(
            config->sample_factor_m,
            result.delta_prefix,
            config->sample_cap
        );

    for (int value = 0; value < 256; value++) {
        beta_left_parity[value] = PARITY8_TABLE[((result.beta >> 8) & 0xFFu) & value];
        beta_right_parity[value] = PARITY8_TABLE[(result.beta & 0xFFu) & value];
    }

    for (int k2 = 0; k2 < 256; k2++) {
        for (int u = 0; u < 256; u++) {
            sign_table[k2][u] = beta_right_parity[round_function((uint8_t)u, (uint8_t)k2)] ? -1 : 1;
        }
    }

    fill_random_sample_without_replacement(
        sample_plaintexts,
        result.sample_count,
        sample_rng_seed(config->seed, iteration_index, key32)
    );

    for (uint32_t i = 0; i < result.sample_count; i++) {
        uint16_t plaintext = sample_plaintexts[i];
        uint16_t ciphertext = encrypt_block_full(plaintext, key);
        sample_plain_parity[i] = parity16((uint16_t)(result.alpha & plaintext));
        sample_cipher_hi[i] = (uint8_t)((ciphertext >> 8) & 0xFFu);
        sample_cipher_lo[i] = (uint8_t)(ciphertext & 0xFFu);
    }

    recovery_start = now_sec();
    for (int k1 = 0; k1 < 256; k1++) {
        int32_t weights[256] = {0};

        for (uint32_t i = 0; i < result.sample_count; i++) {
            uint8_t cipher_hi = sample_cipher_hi[i];
            uint8_t cipher_lo = sample_cipher_lo[i];
            uint8_t u = (uint8_t)(cipher_lo ^ round_function(cipher_hi, (uint8_t)k1));
            uint8_t parity_sum = (uint8_t)(
                sample_plain_parity[i] ^
                beta_left_parity[u] ^
                beta_right_parity[cipher_hi]
            );
            weights[u] += parity_sum ? -1 : 1;
        }

        for (int k2 = 0; k2 < 256; k2++) {
            int32_t score = 0;
            const int8_t *row = sign_table[k2];
            for (int u = 0; u < 256; u++) {
                score += weights[u] * (int32_t)row[u];
            }
            candidate_scores[(k1 << 8) | k2] = score;
        }
    }
    result.recovery_time_sec = now_sec() - recovery_start;
    result.total_time_sec = now_sec() - total_start;

    {
        uint32_t true_index = ((uint32_t)result.true_k1 << 8) | result.true_k2;
        int32_t true_abs = 0;
        int32_t best_abs = -1;
        int32_t min_abs = INT32_MAX;
        int64_t false_score_sum = 0;

        result.true_score = candidate_scores[true_index];
        result.true_bias = (double)result.true_score / (double)result.sample_count;
        true_abs = result.true_score < 0 ? -result.true_score : result.true_score;

        top = (CandidateScore *)calloc(config->top_count, sizeof(CandidateScore));
        if (!top) {
            fprintf(stderr, "Не удалось выделить память под top candidates\n");
            return -1;
        }

        for (uint32_t i = 0; i < config->top_count; i++) {
            top[i].score = INT32_MIN;
        }

        result.true_rank = 1;
        result.best_tie_count = 0;
        result.max_abs_score_signed_min = INT32_MAX;
        result.max_abs_score_signed_max = INT32_MIN;

        for (uint32_t index = 0; index < 65536u; index++) {
            int32_t score = candidate_scores[index];
            int32_t abs_score = score < 0 ? -score : score;

            if (index != true_index) {
                false_score_sum += (int64_t)score;
            }
            if (abs_score > true_abs) {
                result.true_rank++;
            }
            if (abs_score < min_abs) {
                min_abs = abs_score;
            }

            if (abs_score > best_abs) {
                best_abs = abs_score;
                result.best_tie_count = 1;
                result.best_score = score;
                result.best_k1 = (uint8_t)(index >> 8);
                result.best_k2 = (uint8_t)(index & 0xFFu);
                result.max_abs_score_signed_min = score;
                result.max_abs_score_signed_max = score;
            } else if (abs_score == best_abs) {
                result.best_tie_count++;
                if (score < result.max_abs_score_signed_min) {
                    result.max_abs_score_signed_min = score;
                }
                if (score > result.max_abs_score_signed_max) {
                    result.max_abs_score_signed_max = score;
                }
            }

            if (config->top_count > 0) {
                update_top_candidates(top, config->top_count, (uint16_t)index, score);
            }
        }

        result.best_bias = (double)result.best_score / (double)result.sample_count;
        result.false_score_sum = false_score_sum;
        result.false_bias_mean = (double)false_score_sum / ((double)result.sample_count * 65535.0);
        result.min_abs_score_value = (min_abs == INT32_MAX) ? 0 : min_abs;
        result.top_hits = (true_abs == best_abs) ? result.best_tie_count : 0;
    }

    if (summary_csv) {
        char prefix_abs_fraction[64];
        char prefix_signed_fraction[64];
        char prefix_tol_min_fraction[64];
        char prefix_tol_max_fraction[64];
        char true_fraction[64];
        char false_mean_fraction[64];
        char best_fraction[64];
        char max_abs_min_fraction[64];
        char max_abs_max_fraction[64];
        char min_abs_fraction[64];
        uint64_t sample_den = (uint64_t)result.sample_count;
        uint64_t false_mean_den = sample_den * 65535u;

        format_fraction_i64((int64_t)result.delta_prefix_abs_coeff, 65536u, prefix_abs_fraction, sizeof(prefix_abs_fraction));
        format_fraction_i64((int64_t)result.delta_prefix_signed_coeff, 65536u, prefix_signed_fraction, sizeof(prefix_signed_fraction));
        format_fraction_i64((int64_t)result.delta_prefix_signed_coeff_min_tol_2_neg_15, 65536u, prefix_tol_min_fraction, sizeof(prefix_tol_min_fraction));
        format_fraction_i64((int64_t)result.delta_prefix_signed_coeff_max_tol_2_neg_15, 65536u, prefix_tol_max_fraction, sizeof(prefix_tol_max_fraction));
        format_fraction_i64((int64_t)result.true_score, sample_den, true_fraction, sizeof(true_fraction));
        format_fraction_i64(result.false_score_sum, false_mean_den, false_mean_fraction, sizeof(false_mean_fraction));
        format_fraction_i64((int64_t)result.best_score, sample_den, best_fraction, sizeof(best_fraction));
        format_fraction_i64((int64_t)result.max_abs_score_signed_min, sample_den, max_abs_min_fraction, sizeof(max_abs_min_fraction));
        format_fraction_i64((int64_t)result.max_abs_score_signed_max, sample_den, max_abs_max_fraction, sizeof(max_abs_max_fraction));
        format_fraction_i64((int64_t)result.min_abs_score_value, sample_den, min_abs_fraction, sizeof(min_abs_fraction));

        fprintf(
            summary_csv,
            "%s,%s,%s,%.6f,%" PRIu32 ",%" PRIu32 ",%" PRIu64 ",%s,%" PRIu32 ",0x%08" PRIX32 ",0x%02X,0x%02X,0x%02X,0x%02X,0x%04X,0x%04X,%.10f,%" PRId32 ",65536,%s,%.10f,%" PRId32 ",65536,%s,%" PRIu32 ",%.10f,%.10f,%" PRIu32 ",0x%02X,0x%02X,%.10f,%" PRId32 ",%" PRIu32 ",%s,%" PRIu32 ",%.10f,%" PRId64 ",%" PRIu64 ",%s,0x%02X,0x%02X,%.10f,%" PRId32 ",%" PRIu32 ",%s,%" PRIu32 ",%" PRIu32 ",%.10f,%.10f,%" PRId32 ",%" PRId32 ",%s,%s,%.10f,%" PRId32 ",%" PRIu32 ",%s,%.4f,%.4f,%.4f\n",
            path_basename_const(config->output_dir),
            config->series_label ? config->series_label : "default",
            sample_mode_name(config),
            config->sample_factor_m,
            config->sample_cap,
            config->thread_count,
            config->seed,
            config->full_material ? "yes" : "no",
            iteration_index + 1,
            result.key,
            key[0],
            key[1],
            key[2],
            key[3],
            result.alpha,
            result.beta,
            result.delta_prefix,
            result.delta_prefix_abs_coeff,
            prefix_abs_fraction,
            (double)result.delta_prefix_signed_coeff / 65536.0,
            result.delta_prefix_signed_coeff,
            prefix_signed_fraction,
            result.delta_prefix_max_pair_count_tol_2_neg_15,
            (double)result.delta_prefix_signed_coeff_min_tol_2_neg_15 / 65536.0,
            (double)result.delta_prefix_signed_coeff_max_tol_2_neg_15 / 65536.0,
            result.sample_count,
            result.true_k1,
            result.true_k2,
            result.true_bias,
            result.true_score,
            result.sample_count,
            true_fraction,
            result.true_rank,
            result.false_bias_mean,
            result.false_score_sum,
            false_mean_den,
            false_mean_fraction,
            result.best_k1,
            result.best_k2,
            result.best_bias,
            result.best_score,
            result.sample_count,
            best_fraction,
            result.best_tie_count,
            result.top_hits,
            (double)result.max_abs_score_signed_min / (double)result.sample_count,
            (double)result.max_abs_score_signed_max / (double)result.sample_count,
            result.max_abs_score_signed_min,
            result.max_abs_score_signed_max,
            max_abs_min_fraction,
            max_abs_max_fraction,
            (double)result.min_abs_score_value / (double)result.sample_count,
            result.min_abs_score_value,
            result.sample_count,
            min_abs_fraction,
            result.delta_time_sec,
            result.recovery_time_sec,
            result.total_time_sec
        );
        fflush(summary_csv);
    }

    if (config->top_count > 0) {
        if (write_top_candidates(
            config,
            iteration_index,
            result.key,
            result.alpha,
            result.beta,
            top,
            config->top_count,
            result.sample_count
        ) != 0) {
            free(top);
            return -1;
        }
    }

    if (config->save_full_candidates) {
        if (write_full_candidates(
            config,
            iteration_index,
            result.key,
            result.alpha,
            result.beta,
            candidate_scores,
            result.sample_count,
            result.true_k1,
            result.true_k2
        ) != 0) {
            free(top);
            return -1;
        }
    }

    write_iteration_progress(config, iteration_index, &result);

    printf(
        "[%4" PRIu32 "/%" PRIu32 "] key=0x%08" PRIX32
        " delta=%.10f alpha=0x%04X beta=0x%04X"
        " samples=%" PRIu32
        " true=(0x%02X,0x%02X) rank=%" PRIu32
        " true_bias=%.6f best=(0x%02X,0x%02X) best_bias=%.6f"
        " time=%.2fs\n",
        iteration_index + 1,
        config->iterations,
        result.key,
        result.delta_prefix,
        result.alpha,
        result.beta,
        result.sample_count,
        result.true_k1,
        result.true_k2,
        result.true_rank,
        result.true_bias,
        result.best_k1,
        result.best_k2,
        result.best_bias,
        result.total_time_sec
    );

    free(top);
    return 0;
}

static void print_usage(const char *argv0) {
    fprintf(
        stderr,
        "Usage: %s --output-dir DIR [--count N | --fixed-key 0xDEADBEEF] [options]\n"
        "Options:\n"
        "  --count N               Number of true keys to test (default: 1000)\n"
        "  --fixed-key HEX         Use exactly one fixed 32-bit key\n"
        "  --seed N                PRNG seed for random keys (default: 1)\n"
        "  --m VALUE               Sample factor M in ceil(M / delta^2) (default: 100)\n"
        "  --sample-cap N          Cap for sample size (default: 65536)\n"
        "  --series-label TEXT     Label that will be written into CSV files\n"
        "  --full-material         Force sample_count = 65536 for every experiment\n"
        "  --top N                 Save top-N candidates per key (default: 32)\n"
        "  --threads N             Number of CPU threads for spectrum search\n"
        "  --backend MODE          Prefix backend: auto, cpu, cuda (default: auto)\n"
        "  --cuda-threshold-count  In auto mode use CUDA from this key count (default: 128)\n"
        "  --reuse-prefix-summary  Reuse prefix spectrum from another summary.csv\n"
        "  --resume                Continue appending to existing summary.csv\n"
        "  --save-full-candidates  Save all 65536 candidate scores per key\n"
        "  --output-dir DIR        Directory for summary and per-key CSV files\n",
        argv0
    );
}

static int parse_u32(const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 0);
    if (end == value || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return -1;
    }
    *out = (uint32_t)parsed;
    return 0;
}

static int parse_u64(const char *value, uint64_t *out) {
    char *end = NULL;
    unsigned long long parsed = strtoull(value, &end, 0);
    if (end == value || *end != '\0') {
        return -1;
    }
    *out = (uint64_t)parsed;
    return 0;
}

static int parse_double_value(const char *value, double *out) {
    char *end = NULL;
    double parsed = strtod(value, &end);
    if (end == value || *end != '\0' || !isfinite(parsed)) {
        return -1;
    }
    *out = parsed;
    return 0;
}

static int parse_backend_mode(const char *value, KeyRecoveryBackend *out) {
    if (strcmp(value, "auto") == 0) {
        *out = KEY_RECOVERY_BACKEND_AUTO;
        return 0;
    }
    if (strcmp(value, "cpu") == 0) {
        *out = KEY_RECOVERY_BACKEND_CPU;
        return 0;
    }
    if (strcmp(value, "cuda") == 0) {
        *out = KEY_RECOVERY_BACKEND_CUDA;
        return 0;
    }
    return -1;
}

static int parse_i32(const char *value, int32_t *out) {
    char *end = NULL;
    long parsed = strtol(value, &end, 0);
    if (end == value || *end != '\0' || parsed < INT32_MIN || parsed > INT32_MAX) {
        return -1;
    }
    *out = (int32_t)parsed;
    return 0;
}

static int parse_hex_u32_strict(const char *value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 0);
    if (end == value || *end != '\0' || parsed > 0xFFFFFFFFul) {
        return -1;
    }
    *out = (uint32_t)parsed;
    return 0;
}

static int parse_hex_u16_strict(const char *value, uint16_t *out) {
    uint32_t parsed = 0;
    if (parse_hex_u32_strict(value, &parsed) != 0 || parsed > 0xFFFFu) {
        return -1;
    }
    *out = (uint16_t)parsed;
    return 0;
}

static int load_prefix_cache(
    const char *summary_path,
    PrefixCacheEntry **entries_out,
    uint32_t *entry_count_out
) {
    FILE *f = NULL;
    PrefixCacheEntry *entries = NULL;
    uint32_t capacity = 0;
    uint32_t count = 0;
    char line[8192];

    *entries_out = NULL;
    *entry_count_out = 0;

    f = fopen(summary_path, "r");
    if (!f) {
        fprintf(stderr, "Не удалось открыть cache summary %s: %s\n", summary_path, strerror(errno));
        return -1;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *tokens[64] = {0};
        char *saveptr = NULL;
        char *token = NULL;
        size_t token_count = 0;
        PrefixCacheEntry entry = {0};
        double delta_abs_value = 0.0;
        double tol_min_value = 0.0;
        double tol_max_value = 0.0;

        if (strncmp(line, "RUN_ID,", 7) == 0) {
            continue;
        }

        token = strtok_r(line, ",\n\r", &saveptr);
        while (token && token_count < 64) {
            tokens[token_count++] = token;
            token = strtok_r(NULL, ",\n\r", &saveptr);
        }

        if (token_count < 60) {
            fprintf(stderr, "Некорректная строка в cache summary %s\n", summary_path);
            free(entries);
            fclose(f);
            return -1;
        }

        if (parse_u32(tokens[8], &entry.iteration) != 0 ||
            parse_hex_u32_strict(tokens[9], &entry.key) != 0 ||
            parse_hex_u16_strict(tokens[14], &entry.spectrum.alpha) != 0 ||
            parse_hex_u16_strict(tokens[15], &entry.spectrum.beta) != 0 ||
            parse_double_value(tokens[16], &delta_abs_value) != 0 ||
            parse_i32(tokens[21], &entry.spectrum.signed_correlation) != 0 ||
            parse_u32(tokens[24], &entry.spectrum.tol_pair_count) != 0 ||
            parse_double_value(tokens[25], &tol_min_value) != 0 ||
            parse_double_value(tokens[26], &tol_max_value) != 0 ||
            parse_double_value(tokens[57], &entry.delta_time_sec) != 0) {
            fprintf(stderr, "Не удалось распарсить cache summary %s на итерации %s\n", summary_path, tokens[8]);
            free(entries);
            fclose(f);
            return -1;
        }

        entry.spectrum.delta = (float)delta_abs_value;
        entry.spectrum.abs_coeff = (int32_t)llround(delta_abs_value * 65536.0);
        entry.spectrum.exact_pair_count = entry.spectrum.tol_pair_count;
        entry.spectrum.tol_signed_min = (int32_t)llround(tol_min_value * 65536.0);
        entry.spectrum.tol_signed_max = (int32_t)llround(tol_max_value * 65536.0);

        if (count == capacity) {
            uint32_t new_capacity = capacity == 0 ? 1024 : capacity * 2;
            PrefixCacheEntry *new_entries = (PrefixCacheEntry *)realloc(entries, new_capacity * sizeof(PrefixCacheEntry));
            if (!new_entries) {
                fprintf(stderr, "Не удалось расширить память под cache summary\n");
                free(entries);
                fclose(f);
                return -1;
            }
            entries = new_entries;
            capacity = new_capacity;
        }

        entries[count++] = entry;
    }

    fclose(f);

    *entries_out = entries;
    *entry_count_out = count;
    return 0;
}

static uint32_t read_completed_iterations(const char *summary_path) {
    FILE *f = fopen(summary_path, "r");
    char line[4096];
    uint32_t max_iteration = 0;

    if (!f) {
        return 0;
    }

    while (fgets(line, sizeof(line), f) != NULL) {
        char *field = line;
        char *comma = NULL;
        char *end = NULL;
        unsigned long parsed = 0;
        int column = 0;

        if (strncmp(line, "ITERATION,", 10) == 0 || strstr(line, ",ITERATION,") != NULL) {
            continue;
        }

        while (column < 8 && (comma = strchr(field, ',')) != NULL) {
            field = comma + 1;
            column++;
        }
        if (column != 8) {
            continue;
        }

        comma = strchr(field, ',');
        if (!comma) {
            continue;
        }

        parsed = strtoul(field, &end, 10);
        if (end == field || end != comma || parsed > 0xFFFFFFFFul) {
            continue;
        }
        if ((uint32_t)parsed > max_iteration) {
            max_iteration = (uint32_t)parsed;
        }
    }

    fclose(f);
    return max_iteration;
}

int main(int argc, char **argv) {
    Config config = {
        .iterations = 1000,
        .use_fixed_key = false,
        .fixed_key = 0,
        .seed = 1,
        .sample_factor_m = 100.0,
        .sample_cap = 65536,
        .top_count = 32,
        .output_dir = NULL,
        .series_label = NULL,
        .save_full_candidates = false,
        .resume = false,
        .full_material = false,
        .thread_count = 0,
        .reuse_prefix_summary = NULL,
        .backend_mode = KEY_RECOVERY_BACKEND_AUTO,
        .cuda_threshold_count = 128,
    };
    char summary_path[1024];
    char meta_path[1024];
    FILE *summary_csv = NULL;
    FILE *meta = NULL;
    uint16_t *prefix_lookup = NULL;
    uint16_t *sample_plaintexts = NULL;
    uint8_t *sample_plain_parity = NULL;
    uint8_t *sample_cipher_hi = NULL;
    uint8_t *sample_cipher_lo = NULL;
    int32_t *candidate_scores = NULL;
    PrefixCacheEntry *prefix_cache_entries = NULL;
    uint32_t prefix_cache_count = 0;
    KeyRecoveryBackend active_backend = KEY_RECOVERY_BACKEND_CPU;
    KeyRecoveryCudaInfo cuda_info = {0};
    uint64_t rng_state = 0;
    double run_start = 0.0;
    double run_end = 0.0;
    uint32_t start_iteration = 0;

    config.thread_count = default_thread_count();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            uint32_t parsed = 0;
            if (parse_u32(argv[++i], &parsed) != 0 || parsed == 0) {
                fprintf(stderr, "Некорректное значение для --count\n");
                return 1;
            }
            config.iterations = parsed;
        } else if (strcmp(argv[i], "--fixed-key") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.fixed_key) != 0) {
                fprintf(stderr, "Некорректное значение для --fixed-key\n");
                return 1;
            }
            config.use_fixed_key = true;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &config.seed) != 0) {
                fprintf(stderr, "Некорректное значение для --seed\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--m") == 0 && i + 1 < argc) {
            if (parse_double_value(argv[++i], &config.sample_factor_m) != 0 || config.sample_factor_m <= 0.0) {
                fprintf(stderr, "Некорректное значение для --m\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--sample-cap") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.sample_cap) != 0 || config.sample_cap == 0) {
                fprintf(stderr, "Некорректное значение для --sample-cap\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--series-label") == 0 && i + 1 < argc) {
            config.series_label = argv[++i];
        } else if (strcmp(argv[i], "--full-material") == 0) {
            config.full_material = true;
        } else if (strcmp(argv[i], "--top") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.top_count) != 0) {
                fprintf(stderr, "Некорректное значение для --top\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.thread_count) != 0 || config.thread_count == 0) {
                fprintf(stderr, "Некорректное значение для --threads\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--backend") == 0 && i + 1 < argc) {
            if (parse_backend_mode(argv[++i], &config.backend_mode) != 0) {
                fprintf(stderr, "Некорректное значение для --backend, ожидается auto|cpu|cuda\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--cuda-threshold-count") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &config.cuda_threshold_count) != 0 || config.cuda_threshold_count == 0) {
                fprintf(stderr, "Некорректное значение для --cuda-threshold-count\n");
                return 1;
            }
        } else if (strcmp(argv[i], "--reuse-prefix-summary") == 0 && i + 1 < argc) {
            config.reuse_prefix_summary = argv[++i];
        } else if (strcmp(argv[i], "--output-dir") == 0 && i + 1 < argc) {
            config.output_dir = argv[++i];
        } else if (strcmp(argv[i], "--resume") == 0) {
            config.resume = true;
        } else if (strcmp(argv[i], "--save-full-candidates") == 0) {
            config.save_full_candidates = true;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!config.output_dir) {
        fprintf(stderr, "Требуется --output-dir\n");
        print_usage(argv[0]);
        return 1;
    }

    if (config.use_fixed_key) {
        config.iterations = 1;
    }

    init_tables();

    if (select_active_backend(&config, &active_backend, &cuda_info) != 0) {
        return 1;
    }

    if (config.reuse_prefix_summary) {
        if (load_prefix_cache(config.reuse_prefix_summary, &prefix_cache_entries, &prefix_cache_count) != 0) {
            return 1;
        }
        if (prefix_cache_count < config.iterations) {
            fprintf(
                stderr,
                "В cache summary только %" PRIu32 " итераций, а требуется %" PRIu32 "\n",
                prefix_cache_count,
                config.iterations
            );
            free(prefix_cache_entries);
            return 1;
        }
    } else if (active_backend == KEY_RECOVERY_BACKEND_CPU) {
        prefix_lookup = (uint16_t *)malloc(65536u * sizeof(uint16_t));
    }

    {
        uint32_t sample_buffer_size = config.sample_cap;
        if (sample_buffer_size < 65536u) {
            sample_buffer_size = 65536u;
        }
        sample_plaintexts = (uint16_t *)malloc(sample_buffer_size * sizeof(uint16_t));
        sample_plain_parity = (uint8_t *)malloc(sample_buffer_size * sizeof(uint8_t));
        sample_cipher_hi = (uint8_t *)malloc(sample_buffer_size * sizeof(uint8_t));
        sample_cipher_lo = (uint8_t *)malloc(sample_buffer_size * sizeof(uint8_t));
    }
    candidate_scores = (int32_t *)malloc(65536u * sizeof(int32_t));

    if ((!config.reuse_prefix_summary && active_backend == KEY_RECOVERY_BACKEND_CPU && !prefix_lookup) ||
        !sample_plaintexts ||
        !sample_plain_parity ||
        !sample_cipher_hi ||
        !sample_cipher_lo ||
        !candidate_scores) {
        fprintf(stderr, "Не удалось выделить память для эксперимента\n");
        free(prefix_cache_entries);
        return 1;
    }

    snprintf(summary_path, sizeof(summary_path), "%s/summary.csv", config.output_dir);
    if (config.resume) {
        start_iteration = read_completed_iterations(summary_path);
        summary_csv = fopen(summary_path, start_iteration > 0 ? "a" : "w");
    } else {
        summary_csv = fopen(summary_path, "w");
    }
    if (!summary_csv) {
        fprintf(stderr, "Не удалось открыть %s: %s\n", summary_path, strerror(errno));
        free(prefix_cache_entries);
        return 1;
    }

    if (!config.resume || start_iteration == 0) {
        fprintf(
            summary_csv,
            "RUN_ID,SERIES_LABEL,SAMPLE_MODE,SAMPLE_FACTOR_M,SAMPLE_CAP,THREAD_COUNT,SEED,FULL_MATERIAL,ITERATION,TRUE_KEY,K4,K3,K2,K1,PREFIX_ALPHA,PREFIX_BETA,PREFIX_DELTA_ABS_VALUE,PREFIX_DELTA_ABS_NUMERATOR,PREFIX_DELTA_ABS_DENOMINATOR,PREFIX_DELTA_ABS_FRACTION,PREFIX_DELTA_SIGNED_VALUE,PREFIX_DELTA_SIGNED_NUMERATOR,PREFIX_DELTA_SIGNED_DENOMINATOR,PREFIX_DELTA_SIGNED_FRACTION,PREFIX_MAX_PAIR_COUNT_TOL_2_NEG_15,PREFIX_MAX_SIGNED_VALUE_MIN_TOL,PREFIX_MAX_SIGNED_VALUE_MAX_TOL,SAMPLE_COUNT,TRUE_KEY_LAST_ROUND_K1,TRUE_KEY_LAST_ROUND_K2,TRUE_DELTA_SIGNED_VALUE,TRUE_DELTA_SIGNED_NUMERATOR,TRUE_DELTA_SIGNED_DENOMINATOR,TRUE_DELTA_SIGNED_FRACTION,TRUE_RANK,FALSE_DELTA_SIGNED_MEAN_VALUE,FALSE_DELTA_SIGNED_MEAN_NUMERATOR,FALSE_DELTA_SIGNED_MEAN_DENOMINATOR,FALSE_DELTA_SIGNED_MEAN_FRACTION,BEST_GUESS_K1,BEST_GUESS_K2,BEST_DELTA_SIGNED_VALUE,BEST_DELTA_SIGNED_NUMERATOR,BEST_DELTA_SIGNED_DENOMINATOR,BEST_DELTA_SIGNED_FRACTION,BEST_TIE_COUNT,TRUE_IN_BEST_TIES,MAX_ABS_DELTA_SIGNED_MIN_VALUE,MAX_ABS_DELTA_SIGNED_MAX_VALUE,MAX_ABS_DELTA_SIGNED_MIN_NUMERATOR,MAX_ABS_DELTA_SIGNED_MAX_NUMERATOR,MAX_ABS_DELTA_SIGNED_MIN_FRACTION,MAX_ABS_DELTA_SIGNED_MAX_FRACTION,MIN_ABS_DELTA_VALUE,MIN_ABS_DELTA_NUMERATOR,MIN_ABS_DELTA_DENOMINATOR,MIN_ABS_DELTA_FRACTION,DELTA_TIME_SEC,RECOVERY_TIME_SEC,TOTAL_TIME_SEC\n"
        );
        fflush(summary_csv);
    }

    snprintf(meta_path, sizeof(meta_path), "%s/run_config.txt", config.output_dir);
    meta = fopen(meta_path, "w");
    if (!meta) {
        fprintf(stderr, "Не удалось открыть %s: %s\n", meta_path, strerror(errno));
        free(prefix_cache_entries);
        return 1;
    }

    fprintf(meta, "iterations=%" PRIu32 "\n", config.iterations);
    fprintf(meta, "use_fixed_key=%s\n", config.use_fixed_key ? "yes" : "no");
    fprintf(meta, "fixed_key=%s0x%08" PRIX32 "\n", config.use_fixed_key ? "" : "(unused) ", config.fixed_key);
    fprintf(meta, "seed=%" PRIu64 "\n", config.seed);
    fprintf(meta, "sample_factor_m=%.6f\n", config.sample_factor_m);
    fprintf(meta, "sample_cap=%" PRIu32 "\n", config.sample_cap);
    fprintf(meta, "sampling=%s\n", sample_mode_name(&config));
    fprintf(meta, "full_material=%s\n", config.full_material ? "yes" : "no");
    fprintf(meta, "series_label=%s\n", config.series_label ? config.series_label : "default");
    fprintf(meta, "top_count=%" PRIu32 "\n", config.top_count);
    fprintf(meta, "save_full_candidates=%s\n", config.save_full_candidates ? "yes" : "no");
    fprintf(meta, "resume=%s\n", config.resume ? "yes" : "no");
    fprintf(meta, "thread_count=%" PRIu32 "\n", config.thread_count);
    fprintf(meta, "backend_mode=%s\n", backend_mode_name(config.backend_mode));
    fprintf(meta, "active_backend=%s\n", backend_mode_name(active_backend));
    fprintf(meta, "cuda_threshold_count=%" PRIu32 "\n", config.cuda_threshold_count);
    fprintf(meta, "cuda_available=%s\n", cuda_info.available ? "yes" : "no");
    if (cuda_info.status[0] != '\0') {
        fprintf(meta, "cuda_status=%s\n", cuda_info.status);
    }
    fprintf(meta, "reuse_prefix_summary=%s\n", config.reuse_prefix_summary ? config.reuse_prefix_summary : "(none)");
    fprintf(meta, "completed_before_start=%" PRIu32 "\n", start_iteration);
    fclose(meta);
    meta = NULL;

    printf("Выходной каталог: %s\n", config.output_dir);
    printf("Итераций: %" PRIu32 "\n", config.iterations);
    printf("M = %.3f, sample_cap = %" PRIu32 ", top = %" PRIu32 "\n", config.sample_factor_m, config.sample_cap, config.top_count);
    printf("Sampling: %s\n", sample_mode_name(&config));
    printf("Threads: %" PRIu32 "\n", config.thread_count);
    printf("Backend: requested=%s active=%s\n", backend_mode_name(config.backend_mode), backend_mode_name(active_backend));
    if (cuda_info.status[0] != '\0') {
        printf("CUDA status: %s\n", cuda_info.status);
    }
    printf("Series label: %s\n", config.series_label ? config.series_label : "default");
    if (config.reuse_prefix_summary) {
        printf("Reuse prefix summary: %s\n", config.reuse_prefix_summary);
    }
    if (config.use_fixed_key) {
        printf("Фиксированный ключ: 0x%08" PRIX32 "\n", config.fixed_key);
    } else {
        printf("Seed: %" PRIu64 "\n", config.seed);
    }
    if (config.resume) {
        printf("Resume: yes, completed before start = %" PRIu32 "\n", start_iteration);
    }
    printf("\n");

    rng_state = config.seed;
    for (uint32_t skipped = 0; skipped < start_iteration && !config.use_fixed_key; skipped++) {
        (void)splitmix64_next(&rng_state);
    }

    if (start_iteration >= config.iterations) {
        printf("Ничего делать не нужно: уже завершено %" PRIu32 " из %" PRIu32 " итераций.\n", start_iteration, config.iterations);
        fclose(summary_csv);
        free(prefix_lookup);
        free(prefix_cache_entries);
        free(sample_plaintexts);
        free(sample_plain_parity);
        free(sample_cipher_hi);
        free(sample_cipher_lo);
        free(candidate_scores);
        return 0;
    }

    run_start = now_sec();

    for (uint32_t iteration = start_iteration; iteration < config.iterations; iteration++) {
        const PrefixCacheEntry *cached_prefix = NULL;
        uint32_t key32 = 0;

        if (config.reuse_prefix_summary) {
            cached_prefix = &prefix_cache_entries[iteration];
            key32 = cached_prefix->key;
        } else {
            key32 = config.use_fixed_key ? config.fixed_key : (uint32_t)splitmix64_next(&rng_state);
        }
        if (run_iteration(
            &config,
            active_backend,
            iteration,
            key32,
            summary_csv,
            prefix_lookup,
            sample_plaintexts,
            sample_plain_parity,
            sample_cipher_hi,
            sample_cipher_lo,
            candidate_scores,
            cached_prefix
        ) != 0) {
            fclose(summary_csv);
            free(prefix_lookup);
            free(prefix_cache_entries);
            free(sample_plaintexts);
            free(sample_plain_parity);
            free(sample_cipher_hi);
            free(sample_cipher_lo);
            free(candidate_scores);
            return 1;
        }
    }

    run_end = now_sec();
    fclose(summary_csv);

    printf("\nИтог: %.2f секунд на %" PRIu32 " новых итерац.\n", run_end - run_start, config.iterations - start_iteration);
    printf("Сводка: %s\n", summary_path);

    free(prefix_lookup);
    free(prefix_cache_entries);
    free(sample_plaintexts);
    free(sample_plain_parity);
    free(sample_cipher_hi);
    free(sample_cipher_lo);
    free(candidate_scores);
    return 0;
}
