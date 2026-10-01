# Key-Recovery Results for the Original Cipher

## Completed series

Four independent series of the original cipher were completed. Each contains 524,288 experiments, defined by sampled master keys. The series used distinct seeds. Data selection and ranking follow the method described in `key_recovery_study_method.md`.

| Regime | Seed | Mean plaintext count | Top 1 | Top 3 | Top 32 | Mean true rank | Mean true/false absolute-imbalance ratio |
|---|---:|---:|---:|---:|---:|---:|---:|
| `m=1` | 4000039 | 1,122.7 | 2,382 (0.45%) | 2,518 (0.48%) | 4,451 (0.85%) | 23,102.945 | 1.4465 |
| `m=5` | 5000013 | 5,615.3 | 20,187 (3.85%) | 23,311 (4.45%) | 46,148 (8.80%) | 6,654.882 | 2.7370 |
| `m=10` | 6000013 | 11,225.0 | 76,569 (14.60%) | 91,630 (17.48%) | 161,540 (30.81%) | 1,378.254 | 3.7801 |
| Full material | 7000005 | 65,536 | 496,642 (94.73%) | 516,972 (98.60%) | 522,418 (99.64%) | 1.755 | 8.0392 |

The ratio column is the arithmetic mean over experiments of the true absolute imbalance divided by the mean absolute imbalance of false candidates **within the same experiment**. It is not computed from the displayed series means. The top columns give both the count and its percentage of 524,288 experiments.

## Observed pattern

As the amount of material rises across these regimes, the true round-key pair appears more frequently at rank one and its mean rank falls. The mean within-experiment true/false ratio rises from 1.4465 to 8.0392. These observations support statistical separation in this experiment, although the full-material top-1 count remains below the total experiment count. The independent seeds prevent a paired, same-key interpretation across regimes.

## Supplementary tail statistic

The mean maximum false last-four-round product exceeded the mean true product in every regime. The mean per-experiment ratio of maximum false product to true product was approximately 1.44 across the four regimes. Hence this tail statistic did not independently distinguish the true pair from the strongest false candidate.

## Provenance

The figures reproduce the aggregate table and last-four-round table in the locally retained report `from_server/20260916_final_report/final_report.md`. The complete per-experiment data are kept locally and are not included in this lightweight source document.
