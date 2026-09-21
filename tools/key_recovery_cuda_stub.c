#include "key_recovery_cuda_backend.h"

#include <stdio.h>
#include <string.h>

void key_recovery_cuda_query(KeyRecoveryCudaInfo *info) {
    if (!info) {
        return;
    }
    memset(info, 0, sizeof(*info));
    snprintf(info->status, sizeof(info->status), "CUDA backend not built");
}

int key_recovery_cuda_compute_prefix_spectrum(
    uint32_t key32,
    KeyRecoveryKeyMix key_mix,
    PrefixSpectrum *out,
    double *elapsed_sec,
    char *error_buf,
    size_t error_buf_size
) {
    (void)key32;
    (void)key_mix;
    (void)out;
    if (elapsed_sec) {
        *elapsed_sec = 0.0;
    }
    if (error_buf && error_buf_size > 0) {
        snprintf(error_buf, error_buf_size, "CUDA backend not built");
    }
    return -1;
}
