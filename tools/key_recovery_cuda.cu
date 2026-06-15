#include "key_recovery_cuda_backend.h"

#include <cuda_runtime.h>

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const uint8_t PREP_PHASE_TABLE_HOST[256] = {
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

static __constant__ uint8_t d_round_table[256 * 256];
static __device__ __constant__ uint8_t d_prefix10_schedule[10] = {3, 2, 1, 0, 3, 2, 1, 0, 0, 1};

typedef struct {
    int best_abs;
    int exact_pair_count;
    int near_pair_count;
    int exact_signed_min;
    int exact_signed_max;
    int near_signed_min;
    int near_signed_max;
    int canonical_alpha;
    int canonical_signed_correlation;
    int beta;
} DeviceBetaSummary;

static bool g_tables_initialized = false;

static inline double now_sec_cuda(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

static inline void set_error(char *error_buf, size_t error_buf_size, const char *message) {
    if (error_buf && error_buf_size > 0) {
        snprintf(error_buf, error_buf_size, "%s", message);
    }
}

static inline void set_cuda_error(
    char *error_buf,
    size_t error_buf_size,
    const char *context,
    cudaError_t error_code
) {
    if (error_buf && error_buf_size > 0) {
        snprintf(
            error_buf,
            error_buf_size,
            "%s: %s",
            context,
            cudaGetErrorString(error_code)
        );
    }
}

static __device__ __forceinline__ uint8_t round_function_dev(uint8_t x, uint8_t k) {
    return d_round_table[((int)k << 8) | x];
}

static __device__ __forceinline__ uint8_t parity16_dev(uint16_t x) {
    return (uint8_t)(__popc((unsigned)x) & 1u);
}

static __device__ __forceinline__ void encrypt_round_dev(uint16_t *block, uint8_t key_element) {
    uint8_t left = (uint8_t)((*block >> 8) & 0xFFu);
    uint8_t right = (uint8_t)(*block & 0xFFu);
    uint8_t new_left = (uint8_t)(right ^ round_function_dev(left, key_element));
    *block = (uint16_t)(((uint16_t)new_left << 8) | left);
}

static __device__ __forceinline__ uint16_t encrypt_block_prefix10_dev(uint16_t plaintext, uint32_t key32) {
    uint8_t key[4];
    uint16_t block = plaintext;
    key[0] = (uint8_t)((key32 >> 24) & 0xFFu);
    key[1] = (uint8_t)((key32 >> 16) & 0xFFu);
    key[2] = (uint8_t)((key32 >> 8) & 0xFFu);
    key[3] = (uint8_t)(key32 & 0xFFu);

    for (int round = 0; round < 10; round++) {
        encrypt_round_dev(&block, key[d_prefix10_schedule[round]]);
    }
    return block;
}

__global__ static void prefix_lookup_kernel(uint16_t *lookup_table, uint32_t key32) {
    uint32_t x = (uint32_t)(blockIdx.x * blockDim.x + threadIdx.x);
    if (x >= 65536u) {
        return;
    }
    lookup_table[x] = encrypt_block_prefix10_dev((uint16_t)x, key32);
}

__global__ static void init_signs_kernel(
    const uint16_t *lookup_table,
    int *f_buffer,
    int beta_start,
    int batch_size
) {
    uint32_t x = (uint32_t)(blockIdx.x * blockDim.x + threadIdx.x);
    uint32_t row = (uint32_t)blockIdx.y;
    if (x >= 65536u || row >= (uint32_t)batch_size) {
        return;
    }

    int beta = beta_start + (int)row;
    uint16_t y = lookup_table[x];
    f_buffer[row * 65536u + x] = parity16_dev((uint16_t)(beta & y)) ? -1 : 1;
}

__global__ static void fwt_stage_kernel(int *f_buffer, int batch_size, int len) {
    uint32_t global_pair = (uint32_t)(blockIdx.x * blockDim.x + threadIdx.x);
    uint32_t total_pairs = (uint32_t)batch_size * 32768u;
    if (global_pair >= total_pairs) {
        return;
    }

    uint32_t row = global_pair / 32768u;
    uint32_t pair_index = global_pair % 32768u;
    uint32_t group = pair_index / (uint32_t)len;
    uint32_t offset = pair_index % (uint32_t)len;
    uint32_t start = row * 65536u + group * (uint32_t)(len << 1) + offset;

    int a = f_buffer[start];
    int b = f_buffer[start + (uint32_t)len];
    f_buffer[start] = a + b;
    f_buffer[start + (uint32_t)len] = a - b;
}

static __device__ __forceinline__ DeviceBetaSummary beta_summary_identity(int beta) {
    DeviceBetaSummary s;
    s.best_abs = -1;
    s.exact_pair_count = 0;
    s.near_pair_count = 0;
    s.exact_signed_min = INT_MAX;
    s.exact_signed_max = INT_MIN;
    s.near_signed_min = INT_MAX;
    s.near_signed_max = INT_MIN;
    s.canonical_alpha = 0;
    s.canonical_signed_correlation = 0;
    s.beta = beta;
    return s;
}

static __device__ __forceinline__ void merge_min_max(int *min_value, int *max_value, int a, int b) {
    if (a < *min_value) {
        *min_value = a;
    }
    if (b > *max_value) {
        *max_value = b;
    }
}

static __device__ __forceinline__ DeviceBetaSummary beta_summary_combine(
    DeviceBetaSummary current,
    DeviceBetaSummary incoming
) {
    if (current.best_abs < 0) {
        return incoming;
    }
    if (incoming.best_abs < 0) {
        return current;
    }
    if (incoming.best_abs > current.best_abs) {
        DeviceBetaSummary tmp = current;
        current = incoming;
        incoming = tmp;
    }

    if (incoming.best_abs == current.best_abs) {
        current.exact_pair_count += incoming.exact_pair_count;
        current.near_pair_count += incoming.near_pair_count;
        merge_min_max(&current.exact_signed_min, &current.exact_signed_max, incoming.exact_signed_min, incoming.exact_signed_max);
        if (incoming.near_pair_count > 0) {
            if (current.near_pair_count == incoming.near_pair_count) {
                current.near_signed_min = incoming.near_signed_min;
                current.near_signed_max = incoming.near_signed_max;
            } else {
                merge_min_max(&current.near_signed_min, &current.near_signed_max, incoming.near_signed_min, incoming.near_signed_max);
            }
        }
        if (incoming.canonical_alpha < current.canonical_alpha) {
            current.canonical_alpha = incoming.canonical_alpha;
            current.canonical_signed_correlation = incoming.canonical_signed_correlation;
        }
    } else if (incoming.best_abs == current.best_abs - 1) {
        int old_near = current.near_pair_count;
        current.near_pair_count += incoming.exact_pair_count;
        if (incoming.exact_pair_count > 0) {
            if (old_near == 0) {
                current.near_signed_min = incoming.exact_signed_min;
                current.near_signed_max = incoming.exact_signed_max;
            } else {
                merge_min_max(&current.near_signed_min, &current.near_signed_max, incoming.exact_signed_min, incoming.exact_signed_max);
            }
        }
    }

    return current;
}

__global__ static void summarize_kernel(
    const int *f_buffer,
    int beta_start,
    int batch_size,
    DeviceBetaSummary *summaries
) {
    __shared__ DeviceBetaSummary shared[256];

    int row = (int)blockIdx.x;
    int tid = (int)threadIdx.x;
    int beta = beta_start + row;
    DeviceBetaSummary local = beta_summary_identity(beta);

    if (row >= batch_size) {
        return;
    }

    const int *row_ptr = f_buffer + (size_t)row * 65536u;
    for (int alpha = 1 + tid; alpha < 65536; alpha += blockDim.x) {
        int coeff = row_ptr[alpha];
        int abs_coeff = coeff < 0 ? -coeff : coeff;
        int signed_coeff = -coeff;

        if (abs_coeff > local.best_abs) {
            if (local.best_abs == abs_coeff - 1) {
                local.near_pair_count = local.exact_pair_count;
                local.near_signed_min = local.exact_signed_min;
                local.near_signed_max = local.exact_signed_max;
            } else {
                local.near_pair_count = 0;
                local.near_signed_min = INT_MAX;
                local.near_signed_max = INT_MIN;
            }
            local.best_abs = abs_coeff;
            local.exact_pair_count = 1;
            local.exact_signed_min = signed_coeff;
            local.exact_signed_max = signed_coeff;
            local.canonical_alpha = alpha;
            local.canonical_signed_correlation = signed_coeff;
        } else if (abs_coeff == local.best_abs) {
            local.exact_pair_count++;
            if (signed_coeff < local.exact_signed_min) {
                local.exact_signed_min = signed_coeff;
            }
            if (signed_coeff > local.exact_signed_max) {
                local.exact_signed_max = signed_coeff;
            }
            if (alpha < local.canonical_alpha) {
                local.canonical_alpha = alpha;
                local.canonical_signed_correlation = signed_coeff;
            }
        } else if (abs_coeff == local.best_abs - 1) {
            local.near_pair_count++;
            if (local.near_pair_count == 1) {
                local.near_signed_min = signed_coeff;
                local.near_signed_max = signed_coeff;
            } else {
                if (signed_coeff < local.near_signed_min) {
                    local.near_signed_min = signed_coeff;
                }
                if (signed_coeff > local.near_signed_max) {
                    local.near_signed_max = signed_coeff;
                }
            }
        }
    }

    shared[tid] = local;
    __syncthreads();

    for (int stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared[tid] = beta_summary_combine(shared[tid], shared[tid + stride]);
        }
        __syncthreads();
    }

    if (tid == 0) {
        summaries[row] = shared[0];
    }
}

static bool spectrum_is_better_host(
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

static int initialize_cuda_tables(char *error_buf, size_t error_buf_size) {
    uint8_t host_round_table[256 * 256];

    if (g_tables_initialized) {
        return 0;
    }

    for (int k = 0; k < 256; k++) {
        for (int x = 0; x < 256; x++) {
            host_round_table[(k << 8) | x] = PREP_PHASE_TABLE_HOST[(x + k) & 0xFF];
        }
    }

    cudaError_t copy_error = cudaMemcpyToSymbol(
        d_round_table,
        host_round_table,
        sizeof(host_round_table)
    );
    if (copy_error != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaMemcpyToSymbol(d_round_table)", copy_error);
        return -1;
    }

    g_tables_initialized = true;
    return 0;
}

extern "C" void key_recovery_cuda_query(KeyRecoveryCudaInfo *info) {
    cudaError_t error_code;
    int device_count = 0;

    if (!info) {
        return;
    }

    memset(info, 0, sizeof(*info));
    error_code = cudaGetDeviceCount(&device_count);
    if (error_code != cudaSuccess) {
        snprintf(
            info->status,
            sizeof(info->status),
            "cudaGetDeviceCount failed: %s",
            cudaGetErrorString(error_code)
        );
        return;
    }

    info->device_count = device_count;
    if (device_count <= 0) {
        snprintf(info->status, sizeof(info->status), "No CUDA devices available");
        return;
    }

    cudaDeviceProp props;
    error_code = cudaGetDeviceProperties(&props, 0);
    if (error_code != cudaSuccess) {
        snprintf(
            info->status,
            sizeof(info->status),
            "cudaGetDeviceProperties failed: %s",
            cudaGetErrorString(error_code)
        );
        return;
    }

    info->available = true;
    info->selected_device = 0;
    snprintf(info->device_name, sizeof(info->device_name), "%s", props.name);
    snprintf(info->status, sizeof(info->status), "CUDA device 0: %s", props.name);
}

extern "C" int key_recovery_cuda_compute_prefix_spectrum(
    uint32_t key32,
    PrefixSpectrum *out,
    double *elapsed_sec,
    char *error_buf,
    size_t error_buf_size
) {
    const int batch_size_max = 64;
    uint16_t *d_lookup = NULL;
    int *d_f_buffer = NULL;
    DeviceBetaSummary *d_summaries = NULL;
    DeviceBetaSummary host_summaries[batch_size_max];
    PrefixSpectrum result = {0};
    int global_best_abs = -1;
    uint32_t global_exact_pair_count = 0;
    uint32_t global_tol_pair_count = 0;
    int32_t global_tol_signed_min = INT_MAX;
    int32_t global_tol_signed_max = INT_MIN;
    double started_at = 0.0;
    cudaError_t error_code;

    if (!out) {
        set_error(error_buf, error_buf_size, "Output PrefixSpectrum pointer is null");
        return -1;
    }
    if (elapsed_sec) {
        *elapsed_sec = 0.0;
    }

    KeyRecoveryCudaInfo info;
    key_recovery_cuda_query(&info);
    if (!info.available) {
        set_error(error_buf, error_buf_size, info.status);
        return -1;
    }

    error_code = cudaSetDevice(info.selected_device);
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaSetDevice", error_code);
        return -1;
    }
    if (initialize_cuda_tables(error_buf, error_buf_size) != 0) {
        return -1;
    }

    started_at = now_sec_cuda();

    error_code = cudaMalloc((void **)&d_lookup, 65536u * sizeof(uint16_t));
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaMalloc(d_lookup)", error_code);
        goto fail;
    }
    error_code = cudaMalloc((void **)&d_f_buffer, (size_t)batch_size_max * 65536u * sizeof(int));
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaMalloc(d_f_buffer)", error_code);
        goto fail;
    }
    error_code = cudaMalloc((void **)&d_summaries, (size_t)batch_size_max * sizeof(DeviceBetaSummary));
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaMalloc(d_summaries)", error_code);
        goto fail;
    }

    prefix_lookup_kernel<<<256, 256>>>(d_lookup, key32);
    error_code = cudaGetLastError();
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "prefix_lookup_kernel launch", error_code);
        goto fail;
    }

    for (int beta_start = 1; beta_start < 65536; beta_start += batch_size_max) {
        int batch_size = batch_size_max;
        dim3 init_grid((65536u + 255u) / 256u, (unsigned)batch_size, 1u);

        if (beta_start + batch_size > 65536) {
            batch_size = 65536 - beta_start;
            init_grid.y = (unsigned)batch_size;
        }

        init_signs_kernel<<<init_grid, 256>>>(d_lookup, d_f_buffer, beta_start, batch_size);
        error_code = cudaGetLastError();
        if (error_code != cudaSuccess) {
            set_cuda_error(error_buf, error_buf_size, "init_signs_kernel launch", error_code);
            goto fail;
        }

        for (int len = 1; len < 65536; len <<= 1) {
            uint32_t total_pairs = (uint32_t)batch_size * 32768u;
            uint32_t blocks = (total_pairs + 255u) / 256u;
            fwt_stage_kernel<<<blocks, 256>>>(d_f_buffer, batch_size, len);
            error_code = cudaGetLastError();
            if (error_code != cudaSuccess) {
                set_cuda_error(error_buf, error_buf_size, "fwt_stage_kernel launch", error_code);
                goto fail;
            }
        }

        summarize_kernel<<<(unsigned)batch_size, 256>>>(d_f_buffer, beta_start, batch_size, d_summaries);
        error_code = cudaGetLastError();
        if (error_code != cudaSuccess) {
            set_cuda_error(error_buf, error_buf_size, "summarize_kernel launch", error_code);
            goto fail;
        }

        error_code = cudaMemcpy(
            host_summaries,
            d_summaries,
            (size_t)batch_size * sizeof(DeviceBetaSummary),
            cudaMemcpyDeviceToHost
        );
        if (error_code != cudaSuccess) {
            set_cuda_error(error_buf, error_buf_size, "cudaMemcpy(host_summaries)", error_code);
            goto fail;
        }

        for (int i = 0; i < batch_size; i++) {
            const DeviceBetaSummary *summary = &host_summaries[i];

            if (summary->best_abs > global_best_abs) {
                global_best_abs = summary->best_abs;
                result.alpha = (uint16_t)summary->canonical_alpha;
                result.beta = (uint16_t)summary->beta;
                result.delta = (float)summary->best_abs / 65536.0f;
                result.signed_correlation = summary->canonical_signed_correlation;
                global_exact_pair_count = (uint32_t)summary->exact_pair_count;
                global_tol_pair_count = (uint32_t)(summary->exact_pair_count + summary->near_pair_count);
                global_tol_signed_min = summary->exact_signed_min;
                global_tol_signed_max = summary->exact_signed_max;
                if (summary->near_pair_count > 0) {
                    if (summary->near_signed_min < global_tol_signed_min) {
                        global_tol_signed_min = summary->near_signed_min;
                    }
                    if (summary->near_signed_max > global_tol_signed_max) {
                        global_tol_signed_max = summary->near_signed_max;
                    }
                }
            } else {
                if (spectrum_is_better_host(
                    summary->best_abs,
                    (uint16_t)summary->canonical_alpha,
                    (uint16_t)summary->beta,
                    global_best_abs,
                    result.alpha,
                    result.beta
                )) {
                    result.alpha = (uint16_t)summary->canonical_alpha;
                    result.beta = (uint16_t)summary->beta;
                    result.delta = (float)summary->best_abs / 65536.0f;
                    result.signed_correlation = summary->canonical_signed_correlation;
                }

                if (summary->best_abs == global_best_abs) {
                    global_exact_pair_count += (uint32_t)summary->exact_pair_count;
                    global_tol_pair_count += (uint32_t)(summary->exact_pair_count + summary->near_pair_count);
                    if (summary->exact_signed_min < global_tol_signed_min) {
                        global_tol_signed_min = summary->exact_signed_min;
                    }
                    if (summary->exact_signed_max > global_tol_signed_max) {
                        global_tol_signed_max = summary->exact_signed_max;
                    }
                    if (summary->near_pair_count > 0) {
                        if (summary->near_signed_min < global_tol_signed_min) {
                            global_tol_signed_min = summary->near_signed_min;
                        }
                        if (summary->near_signed_max > global_tol_signed_max) {
                            global_tol_signed_max = summary->near_signed_max;
                        }
                    }
                } else if (summary->best_abs == global_best_abs - 1) {
                    global_tol_pair_count += (uint32_t)summary->exact_pair_count;
                    if (summary->exact_signed_min < global_tol_signed_min) {
                        global_tol_signed_min = summary->exact_signed_min;
                    }
                    if (summary->exact_signed_max > global_tol_signed_max) {
                        global_tol_signed_max = summary->exact_signed_max;
                    }
                }
            }
        }
    }

    error_code = cudaDeviceSynchronize();
    if (error_code != cudaSuccess) {
        set_cuda_error(error_buf, error_buf_size, "cudaDeviceSynchronize", error_code);
        goto fail;
    }

    result.abs_coeff = global_best_abs < 0 ? 0 : global_best_abs;
    result.exact_pair_count = global_exact_pair_count;
    result.tol_pair_count = global_tol_pair_count;
    result.tol_signed_min = (global_tol_signed_min == INT_MAX) ? 0 : global_tol_signed_min;
    result.tol_signed_max = (global_tol_signed_max == INT_MIN) ? 0 : global_tol_signed_max;
    *out = result;
    if (elapsed_sec) {
        *elapsed_sec = now_sec_cuda() - started_at;
    }

    cudaFree(d_lookup);
    cudaFree(d_f_buffer);
    cudaFree(d_summaries);
    return 0;

fail:
    cudaFree(d_lookup);
    cudaFree(d_f_buffer);
    cudaFree(d_summaries);
    return -1;
}
