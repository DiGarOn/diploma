# Linear Key-Recovery Study: Cipher and Experimental Method

## Scope

The study uses the project's educational 12-round Feistel cipher. Its block is 16 bits, its master key is 32 bits, and the key schedule supplies an 8-bit round key to each round. The last two rounds use two different key bytes. Recovering their pair is therefore a search over 65,536 hypotheses; it is not, by itself, recovery of the complete master key.

The principal question is how the available number of plaintext-ciphertext pairs affects the ranking of the true pair and the observed linear-relation imbalances under true and false round-key hypotheses. The original cipher and a variant in which round-key addition inside the round function is replaced by XOR were evaluated in separate sets of experiments. The sets use different random seeds and are not paired by master key.

## Linear relation and data selection

For each experimental master key, the first ten rounds form a key-dependent 16-bit permutation. Nonzero input and output masks are searched for the linear relation with the greatest absolute imbalance. The mask search uses a fast Walsh-Hadamard transform. Let delta denote the absolute imbalance of the selected ten-round relation.

Three data regimes draw distinct plaintexts uniformly without replacement from the 16-bit domain. Their sample sizes are `N = min(65536, ceil(m / delta^2))`, with `m` equal to 1, 5, or 10. A fourth regime uses all 65,536 plaintexts. Equivalently, for a given key, its coefficient is large enough to saturate the sample cap; there is no single fixed fourth coefficient common to all key-dependent values of delta. Each series contains 524,288 experiments, with a distinct seed.

## Key ranking and statistics

Each selected plaintext is encrypted with the true master key. For each possible pair of the last two round keys, its ciphertext is partially decrypted through the corresponding two rounds. The selected linear relation is then evaluated between the original plaintext and the recovered intermediate state. The absolute empirical imbalance ranks all 65,536 candidates. The true pair's rank and its inclusion in the top 1, 3, or 32 positions are recorded.

For each experiment, `TRUE_DELTA_ABS` is the absolute empirical imbalance for the true pair, while `FALSE_DELTA_ABS_MEAN` is the mean absolute imbalance over the 65,535 false pairs. The reported true/false ratio is the **mean across experiments of `TRUE_DELTA_ABS / FALSE_DELTA_ABS_MEAN`**. It is not the quotient of the two series-level means. The maximum false imbalance is a separate statistic: a growing mean true/false ratio does not guarantee that the true pair ranks first in every experiment.

A supplementary last-four-round statistic is a product of absolute imbalances for two parts of the tail. It was calculated in both completed experiment sets. It is interpreted separately from the key-ranking statistic and cannot, on its own, establish successful recovery.

## Interpretation limits

Comparisons between data regimes describe independent sets of sampled master keys. Differences between the original and XOR variants likewise cannot be attributed solely to the change of the key-injection operator, because their keys, seeds, and realized sample sizes differ. The findings apply to this educational cipher, not to the security of a standardized block cipher.

## Implementation and data provenance

The cipher implementation is in `src/minigost.c`, its interface in `include/minigost.h`, and the recovery experiment in `tools/key_recovery_experiment.c` and `tools/key_recovery_cuda.cu`. The numerical aggregates and series identifiers are documented in the companion baseline and XOR result summaries. Full per-experiment CSV files are retained locally and are not required to read these summaries.
