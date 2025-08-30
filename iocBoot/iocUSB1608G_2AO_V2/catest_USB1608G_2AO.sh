#!/bin/bash

# ===== 함수 정의 =====

function set_automode() {
  echo "==========[ 초기화: AutoMode ON ]=========="
  caput USB1608G_2AO:AiMode 1
  for i in {5..8}; do
    caput USB1608G_2AO:Threshold:Ai1-Bo$i:AutoMode 1
  done
  sleep 2
}



function set_voltage_and_read() {
  local voltage=$1
  echo ""
  echo "--------- Ao1 = ${voltage}V"
  caput USB1608G_2AO:Ao1 "$voltage"
  sleep 3
  caget USB1608G_2AO:Ai1
  for i in {5..8}; do
    caget USB1608G_2AO:Threshold:Ai1-Bo$i:Calc
  done
  for i in {5..8}; do
    caget USB1608G_2AO:Bo$i
  done
}



function manual_calc_test() {
  echo ""
  echo "==========[ CALC에 고정값으로 직접 넣어서 확인 ]=========="

  # CALC, OUT 필드 수동 설정
  caput USB1608G_2AO:Threshold:Ai1-Bo6:Calc.CALC "B?(A>=2):C"
  caput USB1608G_2AO:Threshold:Ai1-Bo6:Calc.OUT "USB1608G_2AO:Bo6 PP"

  # 테스트 전압 설정
  caput -t USB1608G_2AO:Ao1 2.5
  sleep 3
  caget USB1608G_2AO:Ai1

  # PROC 강제 실행
  caput USB1608G_2AO:Threshold:Ai1-Bo6:Calc.PROC 1

  # 결과 확인
  caget USB1608G_2AO:Threshold:Ai1-Bo6:Calc
  caget USB1608G_2AO:Bo5
  caget USB1608G_2AO:Bo6

  # 다시 0V로 설정
  caput -t USB1608G_2AO:Ao1 0
  sleep 3
  caget USB1608G_2AO:Ai1
}





# 형식: sine_wave_ao1 <진폭> <주파수> <샘플수> <지연시간>
# 예시: sine_wave_ao1 5 1 100 0.1   → 5V 사인파, 1Hz, 100포인트, 0.1초 간격 출력

function sine_wave_ao1() {
  local amplitude=$1   # 최대 전압 (예: 5V)
  local samples=$2     # 전체 샘플 수
  local duration=$3    # 전체 시간 (초)

  # delay = duration / samples
  local delay=$(echo "scale=6; $duration / $samples" | bc -l)

  # frequency = 1 / duration
  local freq=$(echo "scale=6; 1 / $duration" | bc -l)

  echo "===== Ao1에 Sine Wave 출력 시작 ====="
  echo "진폭=${amplitude}V, 샘플수=${samples}, 전체시간=${duration}s, 주기=${freq}Hz, delay=${delay}s"

  for ((i = 0; i < samples; i++)); do
    # 시간 t
    t=$(echo "$i * $delay" | bc -l)

    # 각도 theta = 2πft
    theta=$(echo "2 * 3.141592 * $freq * $t" | bc -l)

    # 사인값 계산
    sin_val=$(python3 -c "import math; print(math.sin($theta))")

    # 전압 = A * sin(theta)
    voltage=$(echo "$amplitude * $sin_val" | bc -l)

    # 출력
    printf "[%03d] Ao1 = %.3f V\n" "$i" "$voltage"
    caput -t USB1608G_2AO:Ao1 "$voltage"

    sleep "$delay"
  done

  echo "===== Sine Wave 출력 종료 ====="
}














# ===== main 함수 정의 =====

function main() {
  set_automode

  local voltages=(0.0 1.11 2.22 3.33 4.44 3.33 2.22 1.11 0.0)

  for v in "${voltages[@]}"; do
    set_voltage_and_read "$v"
  done

  # manual_calc_test
  
  # 진폭 5V, 100 샘플, 50초 동안 사인파 1주기 출력
  sine_wave_ao1 5 100 30

}

# ===== 스크립트 실행 시작 =====
main
