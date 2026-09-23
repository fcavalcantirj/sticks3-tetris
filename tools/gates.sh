#!/usr/bin/env bash
cd "$(dirname "$0")/.." || exit 1
rc=0
gate() { local name="$1" want="$2"; shift 2; local got; got="$(eval "$@" 2>/dev/null | tr -d '[:space:]')"; printf 'gate %-14s want=%-3s got=%s\n' "$name" "$want" "$got"; [ "$got" = "$want" ] || rc=1; }
gate purity 0 "grep -rl 'M5Unified.h\|M5GFX.h\|Arduino.h' src/stackfall | wc -l"
gate no-delay 0 "grep -rn 'delay(' src/stackfall | grep -v '//' | wc -l"
gate text-wrap 1 "grep -rl 'setTextWrap(false' src/hal/sticks3 | wc -l"
gate button-api 0 "grep -rniE 'btnpwr|wasSingleClicked|wasDoubleClicked|setHoldThresh' src | wc -l"
gate g-units 0 "grep -rn '9\.80665\|9\.81' src | wc -l"
gate no-clock 0 "grep -rn 'millis()\|micros()\|esp_timer' src/stackfall | wc -l"
gate no-float 0 "grep -rn 'float\|double' src/stackfall/core src/stackfall/engine | wc -l"
gate rgb565 0 "grep -rnE '0x[0-9A-Fa-f]{6}' src/stackfall/ui src/hal/sticks3 | wc -l"
gate offline 0 "grep -rniE 'wifi|BLEDevice|ArduinoOTA|HTTPClient|MDNS' src | wc -l"
gate no-motor 0 "grep -rniE 'vibrat|haptic|setVibration' src | wc -l"
gate branding 0 "grep -rniE 'tetris|tetrimino|tetromino_official' src release | wc -l"
gate no-upload 0 "grep -rnE '(-t[[:space:]]+(upload|erase))|write_flash|espota' Makefile tools scripts | grep -v 'tools/gates.sh' | wc -l"

golden_writers=$(grep -rlE 'fopen\([^)]*"w"\)|ofstream' test/host |
  sort | tr '\n' ' ')
golden_writer_paths='test/host/sim_main.cpp test/host/test_golden.cpp '
printf 'gate %-14s want=%-3s got=%s\n' "golden-writers" "2" "$golden_writers"
[ "$golden_writers" = "$golden_writer_paths" ] || rc=1
gate golden-write-n 2 "grep -rlE 'fopen\([^)]*\"w\"\)|ofstream' test/host | wc -l"

store_output=$(./build/sf_tests --filter store_ 2>&1)
store_status=$?
printf '%s\n' "$store_output"
store_cases=$(grep -c '^SF_TEST(store_' test/host/test_store.cpp)
store_passes=$(printf '%s\n' "$store_output" | grep -c '^PASS store_')
if [ "$store_status" -ne 0 ] || [ "$store_passes" -ne "$store_cases" ]; then
  rc=1
fi

imu_clamp_output=$(./build/sf_tests --filter test_imu_clamp 2>&1)
imu_clamp_status=$?
printf '%s\n' "$imu_clamp_output"
imu_clamp_passes=$(printf '%s\n' "$imu_clamp_output" |
  grep -c '^PASS test_imu_clamp$')
if [ "$imu_clamp_status" -ne 0 ] || [ "$imu_clamp_passes" -ne 1 ]; then
  rc=1
fi

batt_colour_output=$(./build/sf_tests --filter test_batt_colour 2>&1)
batt_colour_status=$?
printf '%s\n' "$batt_colour_output"
batt_colour_passes=$(printf '%s\n' "$batt_colour_output" |
  grep -c '^PASS test_batt_colour$')
if [ "$batt_colour_status" -ne 0 ] || [ "$batt_colour_passes" -ne 1 ]; then
  rc=1
fi

power_output=$(./build/sf_tests --filter test_power_ 2>&1)
power_status=$?
printf '%s\n' "$power_output"
power_cases=$(grep -c '^SF_TEST(test_power_' test/host/test_power.cpp)
power_passes=$(printf '%s\n' "$power_output" | grep -c '^PASS test_power_')
if [ "$power_status" -ne 0 ] || [ "$power_passes" -ne "$power_cases" ]; then
  rc=1
fi

percentile_output=$(./build/sf_tests --filter test_percentile 2>&1)
percentile_status=$?
printf '%s\n' "$percentile_output"
percentile_passes=$(printf '%s\n' "$percentile_output" |
  grep -c '^PASS test_percentile$')
if [ "$percentile_status" -ne 0 ] || [ "$percentile_passes" -ne 1 ]; then
  rc=1
fi

diag_line_output=$(./build/sf_tests --filter test_diag_line 2>&1)
diag_line_status=$?
printf '%s\n' "$diag_line_output"
diag_line_passes=$(printf '%s\n' "$diag_line_output" |
  grep -c '^PASS test_diag_line$')
if [ "$diag_line_status" -ne 0 ] || [ "$diag_line_passes" -ne 1 ]; then
  rc=1
fi

# Mechanical check: every RuleProfile field must be referenced outside the
# RuleProfile struct block (either elsewhere in profile.h or in any other
# src/stackfall/ file), or be listed in PENDING_PROFILE_FIELDS.
PENDING_PROFILE_FIELDS=(
  "dasMs:consumed by the input/DAS-ARR task"
  "arrMs:consumed by the input/DAS-ARR task"
)

profile_fields=$(sed -n '/struct RuleProfile {/,/^};/p' src/stackfall/rules/profile.h |
  grep -oE '[a-zA-Z_][a-zA-Z0-9_]*[[:space:]]*=' |
  sed 's/[[:space:]]*=//' | sort -u)

profile_no_struct=$(sed '/struct RuleProfile {/,/^};/d' src/stackfall/rules/profile.h)
unwired=()
for field in $profile_fields; do
  in_helpers=$(printf '%s\n' "$profile_no_struct" | grep -c "$field" || true)
  in_other=$(grep -rn "$field" src/stackfall/ --include='*.h' --include='*.cpp' 2>/dev/null |
    grep -v 'src/stackfall/rules/profile.h' | wc -l | tr -d '[:space:]')
  total=$((in_helpers + in_other))
  if [ "$total" -gt 0 ]; then
    continue
  fi
  pending_note=$(printf '%s\n' "${PENDING_PROFILE_FIELDS[@]}" | grep "^$field:" || true)
  if [ -n "$pending_note" ]; then
    continue
  fi
  unwired+=("$field")
  rc=1
done

if [ ${#unwired[@]} -gt 0 ]; then
  printf 'gate %-14s want=%-3s got=%s\n' "profile-fields" "0" "${#unwired[@]} unwired: ${unwired[*]}"
else
  printf 'gate %-14s want=%-3s got=%s\n' "profile-fields" "0" "0"
fi

golden_output=$(./build/sf_tests --filter golden_ 2>&1)
golden_status=$?
printf '%s\n' "$golden_output"
golden_passes=$(printf '%s\n' "$golden_output" | grep -c '^PASS golden_')
if [ "$golden_status" -ne 0 ] || [ "$golden_passes" -ne 8 ]; then
  rc=1
fi

[ $rc -eq 0 ] || { echo 'GATES FAIL'; exit 1; }
echo "GATES PASS"
