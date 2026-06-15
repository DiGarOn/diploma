#ifndef KEY_RECOVERY_CUDA_BACKEND_H
#define KEY_RECOVERY_CUDA_BACKEND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t alpha;
    uint16_t beta;
    float delta;
    int32_t signed_correlation;
    int32_t abs_coeff;
    uint32_t exact_pair_count;
    uint32_t tol_pair_count;
    int32_t tol_signed_min;
    int32_t tol_signed_max;
} PrefixSpectrum;

typedef enum {
    KEY_RECOVERY_BACKEND_AUTO = 0,
    KEY_RECOVERY_BACKEND_CPU = 1,
    KEY_RECOVERY_BACKEND_CUDA = 2
} KeyRecoveryBackend;

typedef struct {
    bool available;
    int device_count;
    int selected_device;
    char device_name[256];
    char status[256];
} KeyRecoveryCudaInfo;

void key_recovery_cuda_query(KeyRecoveryCudaInfo *info);
int key_recovery_cuda_compute_prefix_spectrum(
    uint32_t key32,
    PrefixSpectrum *out,
    double *elapsed_sec,
    char *error_buf,
    size_t error_buf_size
);

#ifdef __cplusplus
}
#endif

#endif
