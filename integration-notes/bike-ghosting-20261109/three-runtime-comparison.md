# Three-runtime V557 comparison — November 9, 2026

Same installed631a180b/bfc0340e DLL; V557 Medium/50%,90Hz, adaptive pacingON. Natural gameplay, unequal duration/resolution, not matched-workload benchmark. Selected records have accepted_synthetic>10 and no settings transition. Count fields below are selected-window totals unless labeled fullrun. VDXR missing values are unavailable, never zero. Successful xrEndFrame does not confirm displayed/unique content. SteamVR compositor estimate includes repeats and excludes headset streaming/decode latency.

## Run and rates

| Metric | PSVR2 / SteamVR | Quest3 / VDXR | Quest3 / Steam Link |
|---|---:|---:|---:|
| elapsed_s | 176.350 | 279.463 | 183.069 |
| seconds | 132.793 | 235.584 | 137.822 |
| selected_rows | 132 | 235 | 137 |
| capture_fps | 35.461 | 45.173 | 41.691 |
| accepted_real_fps | 35.439 | 44.927 | 41.329 |
| accepted_synthetic_fps | 35.416 | 44.485 | 41.162 |
| app_repeat_fps | 18.631 | 0.166 | 4.600 |
| compositor_estimate_fps | 85.150 | — | 88.002 |
| compositor_coverage_seconds | 132.793 | 0 | 137.822 |
| app_gpu_ms | 8.819 | — | 6.321 |
| compositor_gpu_ms | 0.331 | — | 0.077 |
| mispresented_reports_percent | 5.911 | — | 0.000 |
| zero_present_records_percent | 2.556 | — | 0.000 |
| dropped_reports_percent | 0.042 | — | 0.149 |
| Fresh accepted/s | 70.855 | 89.412 | 82.491 |
| Eye resolution | [[3144, 3000]] | [[2856, 3088]] | [[2856, 3088]] |
| Input resolution | [[1048, 1000], [1048, 1000]] | [[952, 1029], [952, 1029]] | [[952, 1029], [952, 1029]] |
| Package active | [True] | [False] | [True] |

## All timing aggregates: mean / maximum milliseconds (sample count)

| Metric | PSVR2 / SteamVR | Quest3 / VDXR | Quest3 / Steam Link |
|---|---:|---:|---:|
| capture_enqueue_to_end_ms | 14.002 / 18.738 (n=294) | — | 11.381 / 16.396 (n=354) |
| capture_enqueue_to_start_ms | 13.858 / 18.595 (n=294) | — | 11.245 / 16.259 (n=354) |
| capture_gpu_span_ms | 0.144 / 1.652 (n=294) | — | 0.136 / 0.543 (n=354) |
| capture_wait_ms | — | — | — |
| display_gpu_lag_boundary_ms | 3.468 / 7.899 (n=76) | — | 2.329 / 5.712 (n=32) |
| display_gpu_lag_handoff_ms | 4.813 / 17.064 (n=177) | — | 6.383 / 18.231 (n=330) |
| display_gpu_lag_target_ms | 4.454 / 10.576 (n=115) | — | 2.834 / 8.712 (n=86) |
| display_gpu_lead_boundary_ms | 8.902 / 13.472 (n=307) | — | 9.267 / 21.334 (n=356) |
| display_gpu_lead_handoff_ms | 6.870 / 10.117 (n=206) | — | 2.029 / 7.783 (n=58) |
| display_gpu_lead_target_ms | 7.318 / 10.824 (n=268) | — | 7.706 / 18.334 (n=302) |
| display_handoff_lag_target_ms | 0.105 / 0.632 (n=65) | — | 0.156 / 0.280 (n=5) |
| display_handoff_lead_target_ms | 2.806 / 17.121 (n=318) | — | 10.634 / 19.697 (n=383) |
| display_probe_width_ms | 0.001 / 0.140 (n=383) | — | 0.002 / 0.015 (n=388) |
| display_queue_ms | 6.948 / 19.687 (n=383) | — | 6.227 / 19.823 (n=388) |
| display_remaining_ms | 4.978 / 20.122 (n=383) | — | 13.438 / 21.353 (n=388) |
| generation_after_capture_ready_ms | 0.204 / 2.674 (n=294) | — | 0.284 / 2.102 (n=354) |
| generation_before_capture_ready_ms | 12.901 / 17.304 (n=294) | — | 6.407 / 15.846 (n=354) |
| generation_calibration_cpu_ms | — | — | — |
| generation_enqueue_to_end_ms | 17.073 / 29.217 (n=4702) | 5.456 / 36.390 (n=7260) | 10.099 / 44.299 (n=5673) |
| generation_enqueue_to_start_ms | 13.994 / 23.894 (n=4702) | 2.219 / 20.401 (n=7260) | 7.125 / 40.149 (n=5673) |
| generation_gpu_ms | 0.455 / 2.223 (n=4703) | 0.444 / 1.996 (n=10490) | 0.397 / 2.336 (n=5695) |
| generation_queue_inputs_pending_ms | 14.038 / 23.894 (n=4687) | 2.983 / 20.401 (n=5004) | 7.999 / 40.149 (n=5006) |
| generation_queue_inputs_ready_ms | 0.306 / 0.358 (n=15) | 0.525 / 13.129 (n=2256) | 0.564 / 2.403 (n=667) |
| generation_span_ms | 3.078 / 7.238 (n=4703) | 3.083 / 29.008 (n=10490) | 2.974 / 9.812 (n=5695) |
| pacing_budget_ms | 22.222 / 22.222 (n=4705) | 22.222 / 22.222 (n=10506) | 22.280 / 24.996 (n=5743) |
| phase_step_abs_ms | 0.155 / 11.300 (n=1386) | — | 1.731 / 16.022 (n=390) |
| prepare_ahead_cpu_ms | 0.320 / 7.359 (n=6867) | — | 0.404 / 6.422 (n=934) |
| prepare_ahead_lead_ms | 6.448 / 20.536 (n=2754) | — | 9.802 / 24.890 (n=449) |
| prepare_ahead_overrun_ms | 3.218 / 5.302 (n=14) | — | 2.248 / 2.654 (n=5) |
| prepare_before_hold_ms | 0.649 / 212.560 (n=11883) | — | 0.825 / 252.036 (n=12003) |
| producer_cpu_hold_ms | 7.496 / 11.961 (n=4705) | 11.607 / 16.840 (n=10506) | 11.289 / 15.580 (n=5742) |
| producer_gpu_hold_ms | 5.549 / 6.759 (n=4704) | — | 1.382 / 6.419 (n=5742) |
| real_handoff_gap_ms | 11.141 / 213.857 (n=4706) | — | 10.886 / 262.620 (n=5696) |
| real_remaining_ms | 5.027 / 20.790 (n=4697) | — | 13.964 / 21.664 (n=5691) |
| real_xr_end_ms | 1.195 / 19.354 (n=4706) | — | 2.138 / 38.917 (n=5696) |
| repeat_handoff_gap_ms | 11.199 / 44.051 (n=2474) | — | 12.392 / 25.242 (n=634) |
| repeat_remaining_ms | 4.860 / 20.805 (n=2473) | — | 12.213 / 21.495 (n=634) |
| repeat_xr_end_ms | 0.859 / 3.259 (n=2474) | — | 1.650 / 11.309 (n=634) |
| source_age_ms | 28.552 / 256.375 (n=11883) | 24.168 / 199.504 (n=21103) | 20.275 / 252.311 (n=12003) |
| submit_hold_ms | 8.457 / 17.697 (n=11883) | — | 1.068 / 19.576 (n=12003) |
| submit_margin_ms | 2.664 / 3.000 (n=11870) | — | 2.948 / 3.000 (n=11995) |
| submit_probe_ms | 0.006 / 0.220 (n=11883) | — | 0.005 / 0.118 (n=12003) |
| synthetic_handoff_gap_ms | 11.195 / 44.051 (n=4703) | — | 11.876 / 59.036 (n=5673) |
| synthetic_remaining_ms | 4.972 / 20.467 (n=4700) | — | 13.354 / 21.403 (n=5670) |
| synthetic_xr_end_ms | 0.921 / 7.343 (n=4703) | — | 1.422 / 16.377 (n=5673) |
| xr_end_ms | 1.016 / 19.353 (n=11883) | 1.283 / 21.684 (n=21103) | 1.774 / 38.917 (n=12003) |
| xr_wait_ms | 0.921 / 15.133 (n=11883) | 9.145 / 11.073 (n=21103) | 7.610 / 53.809 (n=12003) |

## All event counters: count (per second)

| Metric | PSVR2 / SteamVR | Quest3 / VDXR | Quest3 / Steam Link |
|---|---:|---:|---:|
| accepted_real | 4706 (35.4386/s) | 10584 (44.9266/s) | 5696 (41.3287/s) |
| accepted_repeat | 2474 (18.6305/s) | 39 (0.1655/s) | 634 (4.6001/s) |
| accepted_synthetic | 4703 (35.4161/s) | 10480 (44.4852/s) | 5673 (41.1618/s) |
| allocator_busy | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| begin_discarded | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| capture | 4709 (35.4612/s) | 10642 (45.1728/s) | 5746 (41.6915/s) |
| capture_no_slot | 0 (0.0000/s) | 10 (0.0424/s) | 3 (0.0218/s) |
| capture_probe_invalid | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| capture_probe_paired | 294 (2.2140/s) | 0 (0.0000/s) | 354 (2.5685/s) |
| capture_probe_queued | 294 (2.2140/s) | 0 (0.0000/s) | 359 (2.6048/s) |
| capture_probe_skipped | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| capture_timeout | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| cycle | 11882 (89.4777/s) | 21103 (89.5774/s) | 11994 (87.0254/s) |
| display_both_late | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_boundary_ambiguous | 4 (0.0301/s) | 0 (0.0000/s) | 4 (0.0290/s) |
| display_boundary_future | 66 (0.4970/s) | 0 (0.0000/s) | 322 (2.3363/s) |
| display_cpu_and_gpu_early | 305 (2.2968/s) | 0 (0.0000/s) | 353 (2.5613/s) |
| display_cpu_only_late | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_gpu_after_handoff | 176 (1.3254/s) | 0 (0.0000/s) | 302 (2.1912/s) |
| display_gpu_before_handoff | 204 (1.5362/s) | 0 (0.0000/s) | 32 (0.2322/s) |
| display_gpu_early_boundary | 305 (2.2968/s) | 0 (0.0000/s) | 353 (2.5613/s) |
| display_gpu_late_boundary | 74 (0.5573/s) | 0 (0.0000/s) | 31 (0.2249/s) |
| display_gpu_only_late | 74 (0.5573/s) | 0 (0.0000/s) | 31 (0.2249/s) |
| display_handoff_ambiguous | 3 (0.0226/s) | 0 (0.0000/s) | 54 (0.3918/s) |
| display_handoff_early_target | 169 (1.2727/s) | 0 (0.0000/s) | 375 (2.7209/s) |
| display_handoff_late_target | 5 (0.0377/s) | 0 (0.0000/s) | 1 (0.0073/s) |
| display_probe_invalid | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_probe_queued | 383 (2.8842/s) | 0 (0.0000/s) | 388 (2.8152/s) |
| display_probe_rejected | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_probe_skipped | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_real | 144 (1.0844/s) | 0 (0.0000/s) | 168 (1.2190/s) |
| display_real_gpu_late | 0 (0.0000/s) | 0 (0.0000/s) | 2 (0.0145/s) |
| display_record_dropped | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_repeat | 88 (0.6627/s) | 0 (0.0000/s) | 18 (0.1306/s) |
| display_repeat_gpu_late | 5 (0.0377/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| display_synthetic | 151 (1.1371/s) | 0 (0.0000/s) | 202 (1.4657/s) |
| display_synthetic_gpu_late | 69 (0.5196/s) | 0 (0.0000/s) | 29 (0.2104/s) |
| display_target_ambiguous | 209 (1.5739/s) | 0 (0.0000/s) | 12 (0.0871/s) |
| empty_end | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| fallback_repeat | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| generate_failed | 0 (0.0000/s) | 12 (0.0509/s) | 0 (0.0000/s) |
| generated_queued | 4704 (35.4236/s) | 10480 (44.4852/s) | 5673 (41.1618/s) |
| generation_calibration_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| generation_calibration_invalid | 1 (0.0075/s) | 3230 (13.7106/s) | 22 (0.1596/s) |
| generation_capture_pending | 4688 (35.3031/s) | 5016 (21.2918/s) | 5024 (36.4529/s) |
| generation_consumer_pending | 15 (0.1130/s) | 2475 (10.5058/s) | 3 (0.0218/s) |
| generation_guides_pending | 4677 (35.2203/s) | 3507 (14.8864/s) | 4842 (35.1323/s) |
| generation_probe | 4702 (35.4085/s) | 7260 (30.8170/s) | 5673 (41.1618/s) |
| generator_busy | 0 (0.0000/s) | 12 (0.0509/s) | 0 (0.0000/s) |
| input_left | 4709 (35.4612/s) | 10539 (44.7356/s) | 5749 (41.7133/s) |
| input_right | 4709 (35.4612/s) | 10539 (44.7356/s) | 5749 (41.7133/s) |
| marker | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| overlay_source_pending | 4719 (35.5365/s) | 0 (0.0000/s) | 6961 (50.5072/s) |
| overlay_source_ready | 32 (0.2410/s) | 0 (0.0000/s) | 193 (1.4004/s) |
| paced_cycle | 11883 (89.4852/s) | 0 (0.0000/s) | 12003 (87.0907/s) |
| pacer_no_reading | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| pacer_nonpositive | 13 (0.0979/s) | 0 (0.0000/s) | 8 (0.0580/s) |
| pacer_out_of_range | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| pacer_timer_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| pacer_unavailable | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| pair_gap | 0 (0.0000/s) | 8 (0.0340/s) | 21 (0.1524/s) |
| pair_rejected | 1 (0.0075/s) | 8 (0.0340/s) | 23 (0.1669/s) |
| paused | 0 (0.0000/s) | 3 (0.0127/s) | 0 (0.0000/s) |
| phase_earlier | 32 (0.2410/s) | 0 (0.0000/s) | 17 (0.1233/s) |
| phase_gpu_pending | 5348 (40.2732/s) | 0 (0.0000/s) | 10934 (79.3343/s) |
| phase_gpu_recovery | 50 (0.3765/s) | 0 (0.0000/s) | 225 (1.6325/s) |
| phase_jump | 6 (0.0452/s) | 0 (0.0000/s) | 32 (0.2322/s) |
| phase_later | 1348 (10.1511/s) | 0 (0.0000/s) | 341 (2.4742/s) |
| prepare_ahead_attempt | 9463 (71.2613/s) | 0 (0.0000/s) | 1258 (9.1277/s) |
| prepare_ahead_cancelled | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| prepare_ahead_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| prepare_ahead_overrun | 14 (0.1054/s) | 0 (0.0000/s) | 5 (0.0363/s) |
| prepare_ahead_pending | 2744 (20.6638/s) | 0 (0.0000/s) | 360 (2.6121/s) |
| prepare_ahead_queued | 2754 (20.7391/s) | 0 (0.0000/s) | 449 (3.2578/s) |
| prepare_ahead_ready | 10 (0.0753/s) | 0 (0.0000/s) | 89 (0.6458/s) |
| prepare_ahead_used | 2754 (20.7391/s) | 0 (0.0000/s) | 449 (3.2578/s) |
| presentation_application_wait | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| presentation_copy | 11883 (89.4852/s) | 0 (0.0000/s) | 12003 (87.0907/s) |
| presentation_pending | 5348 (40.2732/s) | 0 (0.0000/s) | 10934 (79.3343/s) |
| presentation_ready | 6535 (49.2120/s) | 0 (0.0000/s) | 1069 (7.7564/s) |
| presentation_runtime_wait | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| presentation_sync_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| primed | 2 (0.0151/s) | 15 (0.0637/s) | 24 (0.1741/s) |
| producer_gpu_device_lost | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| producer_gpu_missing | 0 (0.0000/s) | 0 (0.0000/s) | 3 (0.0218/s) |
| producer_gpu_ready | 21 (0.1581/s) | 0 (0.0000/s) | 2359 (17.1163/s) |
| producer_gpu_released | 1448 (10.9042/s) | 0 (0.0000/s) | 3300 (23.9440/s) |
| producer_gpu_timeout | 3233 (24.3462/s) | 0 (0.0000/s) | 79 (0.5732/s) |
| producer_gpu_wait | 4683 (35.2654/s) | 0 (0.0000/s) | 3380 (24.5244/s) |
| producer_gpu_wait_failed | 2 (0.0151/s) | 0 (0.0000/s) | 1 (0.0073/s) |
| real_gpu_pending | 837 (6.3030/s) | 10500 (44.5701/s) | 4797 (34.8058/s) |
| real_gpu_ready | 3869 (29.1356/s) | 85 (0.3608/s) | 899 (6.5229/s) |
| settings_change | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_invalid | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_marker_duplicate | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_marker_recorded | 1764 (13.2838/s) | 0 (0.0000/s) | 2154 (15.6289/s) |
| source_marker_replay | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_marker_signal_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_marker_skipped | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| source_paired | 294 (2.2140/s) | 0 (0.0000/s) | 354 (2.5685/s) |
| source_record_dropped | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| submit_rejected | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| synthesis_consumer_deferred | 4705 (35.4311/s) | 0 (0.0000/s) | 5696 (41.3287/s) |
| synthesis_consumer_late_pending | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| synthesis_input_fallback | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| synthesis_private | 4706 (35.4386/s) | 0 (0.0000/s) | 5697 (41.3360/s) |
| synthesis_queue_fallback | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| synthesis_shared | 0 (0.0000/s) | 10495 (44.5489/s) | 0 (0.0000/s) |
| synthesis_sync_failed | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |
| synthetic_gpu_pending | 3963 (29.8435/s) | 10480 (44.4852/s) | 5443 (39.4930/s) |
| synthetic_gpu_pending_before_hold | 4701 (35.4010/s) | 10480 (44.4852/s) | 5645 (40.9587/s) |
| synthetic_gpu_ready | 740 (5.5726/s) | 0 (0.0000/s) | 230 (1.6688/s) |
| waiting_inputs | 13 (0.0979/s) | 4 (0.0170/s) | 10 (0.0726/s) |
| xr_error | 0 (0.0000/s) | 0 (0.0000/s) | 0 (0.0000/s) |

## All compositor counters

| Metric | PSVR2 / SteamVR | Quest3 / VDXR | Quest3 / Steam Link |
|---|---:|---:|---:|
| records | 11893 | — | 12119 |
| presented_records | 11589 | — | 12119 |
| zero_present_records | 304 | — | 0 |
| history_holes | 0 | — | 0 |
| dropped_reports | 5 | — | 18 |
| mispresented_reports | 703 | — | 0 |
| extra_presents | 345 | — | 0 |

## Interpretation

VDXR supplies most fresh submissions (89.41/s), then SteamLink82.49,PSVR270.86. SteamLink compositor88.00 vsPSVR285.15; noVDXR compositor stats. SteamLink zero mispresent/zero-present records inselectedwindows doesnotprove perfect headsetdelivery orzero reprojection. Its dropped reports are18. Counters overlap; do not suminto loss.

Main measured separation precedes reconstruction: PSVR2 pairedcapturewait12.901ms vsSteamLink6.407; producerGPUhold5.549ms vs1.382. Activepack/composition0.455/0.444/0.397ms and stereo span3.078/3.083/2.974ms are similar; activeGPUtiming excludes driver-ownedOFAwork, span includes waits/OFA. These are observations, notproof of rootcause.

VDXR generation enqueue timings are partial:3230 generation_calibration_invalid vs7260 valid generation_probe observations. Do not equate rejected diagnostic samples with GPUexecution failures or infer impossiblezero capturelatency. No source-stage correlation forVDXR. Sourceage isnot motion-to-photon/networklatency.

PSVR2 outputs3144x3000 vsbothQuest2856x3088 (~6.95%morepixels). RT/NR matching acrossruns isnot independentlyestablished; RR=false in SteamVRsourceprobes butVDXR hasnone. Fullcounts/headers/timingaggregates also saved inthree-runtime-comparison.json.
