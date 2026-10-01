# Key-Recovery Results for the XOR Key-Injection Variant

## Completed series

The round-key injection within the round function was changed to XOR. Four independent series were completed, each with 524,288 experiments. The keys and seeds differ between series and from those used for the original cipher. The same data-selection and ranking definitions are given in `key_recovery_study_method.md`.

| Regime | Seed | Mean plaintext count | Top 1 | Top 3 | Top 32 | Mean true rank | Mean true/false absolute-imbalance ratio |
|---|---:|---:|---:|---:|---:|---:|---:|
| `m=1` | 94220958 | 572.0 | 5,256 (1.0025%) | 5,256 (1.0025%) | 6,058 (1.1555%) | 22,770.40 | 1.4384 |
| `m=5` | 95210932 | 2,856.4 | 43,423 (8.2823%) | 43,423 (8.2823%) | 53,466 (10.1978%) | 6,975.12 | 2.6650 |
| `m=10` | 96210932 | 5,711.7 | 132,020 (25.1808%) | 132,032 (25.1831%) | 160,985 (30.7055%) | 1,695.18 | 3.6083 |
| Full material | 97240924 | 65,536 | 516,581 (98.5300%) | 516,581 (98.5300%) | 518,622 (98.9193%) | 2.288 | 8.2255 |

The ratio is calculated within each experiment and then averaged across the series. It is not the ratio of two aggregate means. Top counts are out of 524,288 experiments.

## Observed pattern

The top-1 percentage rises from 1.0025% in the smallest adaptive regime to 98.5300% on full material. The mean true/false absolute-imbalance ratio rises from 1.4384 to 8.2255. Even with all plaintexts, the true pair did not take first place in 7,707 experiments. These are descriptive findings for independent series; they do not establish a controlled causal effect of the XOR substitution relative to the original cipher.

## Supplementary tail statistic

In the saved XOR summaries, the true product, maximum false product, and mean false product of the last-four-round statistic coincide in every recorded experiment. Their stored ratios equal 1. This statistic therefore did not separate true and false candidate pairs in these data. The primary recovery conclusions rely on candidate ranks and empirical imbalances instead.

## Provenance

The values reproduce the final aggregate report retained locally at `results/key_recovery_suite/20260922_xor_final_report/full_report.md`. Complete per-experiment CSV files remain local and are not part of this lightweight source document.
