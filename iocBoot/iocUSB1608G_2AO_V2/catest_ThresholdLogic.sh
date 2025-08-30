#!/bin/bash

# ThresholdLogic Controller 테스트 스크립트
# 이 스크립트는 ThresholdLogicController의 모든 기능을 테스트합니다.

# ===== 색상 정의 =====
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# ===== PV 이름 정의 =====
PREFIX="USB1608G_2AO:"
CONTROLLER="ThresholdLogic1"

THRESHOLD_PV="${PREFIX}${CONTROLLER}Threshold"
HYSTERESIS_PV="${PREFIX}${CONTROLLER}Hysteresis"
CURRENT_VALUE_PV="${PREFIX}${CONTROLLER}CurrentValue"
OUTPUT_STATE_PV="${PREFIX}${CONTROLLER}OutputState"
ENABLE_PV="${PREFIX}${CONTROLLER}Enable"
UPDATE_RATE_PV="${PREFIX}${CONTROLLER}UpdateRate"
ALARM_STATE_PV="${PREFIX}${CONTROLLER}AlarmState"
RESET_PV="${PREFIX}${CONTROLLER}Reset"

# 테스트용 아날로그 출력 (입력 시뮬레이션용)
AO_PV="${PREFIX}Ao1"

# ===== 유틸리티 함수 =====

function print_header() {
    echo -e "${CYAN}========================================${NC}"
    echo -e "${CYAN}$1${NC}"
    echo -e "${CYAN}========================================${NC}"
}

function print_section() {
    echo -e "\n${YELLOW}--- $1 ---${NC}"
}

function print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

function print_error() {
    echo -e "${RED}✗ $1${NC}"
}

function print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

function wait_for_user() {
    echo -e "${PURPLE}Press Enter to continue...${NC}"
    read
}

function check_pv_connection() {
    local pv=$1
    local timeout=5
    
    if timeout $timeout caget "$pv" > /dev/null 2>&1; then
        print_success "PV 연결 성공: $pv"
        return 0
    else
        print_error "PV 연결 실패: $pv"
        return 1
    fi
}

function get_pv_value() {
    local pv=$1
    caget -t "$pv" 2>/dev/null
}

function set_pv_value() {
    local pv=$1
    local value=$2
    caput -t "$pv" "$value" > /dev/null 2>&1
}

function monitor_pvs() {
    local duration=$1
    print_info "실시간 모니터링 시작 (${duration}초간)..."
    timeout "$duration" camonitor "$CURRENT_VALUE_PV" "$OUTPUT_STATE_PV" "$THRESHOLD_PV" 2>/dev/null || true
}

# ===== 테스트 함수들 =====

function test_pv_connections() {
    print_header "1. PV 연결 테스트"
    
    local pvs=(
        "$THRESHOLD_PV"
        "$HYSTERESIS_PV"
        "$CURRENT_VALUE_PV"
        "$OUTPUT_STATE_PV"
        "$ENABLE_PV"
        "$UPDATE_RATE_PV"
        "$ALARM_STATE_PV"
        "$AO_PV"
    )
    
    local failed=0
    for pv in "${pvs[@]}"; do
        if ! check_pv_connection "$pv"; then
            ((failed++))
        fi
    done
    
    if [ $failed -eq 0 ]; then
        print_success "모든 PV 연결 성공!"
    else
        print_error "$failed 개의 PV 연결 실패"
        echo "IOC가 실행 중인지 확인하세요."
        exit 1
    fi
}

function test_initial_setup() {
    print_header "2. 초기 설정 테스트"
    
    print_section "기본 설정값 확인"
    echo "임계값: $(get_pv_value $THRESHOLD_PV) V"
    echo "히스테리시스: $(get_pv_value $HYSTERESIS_PV) V"
    echo "업데이트 주기: $(get_pv_value $UPDATE_RATE_PV) Hz"
    echo "활성화 상태: $(get_pv_value $ENABLE_PV)"
    echo "현재값: $(get_pv_value $CURRENT_VALUE_PV) V"
    echo "출력 상태: $(get_pv_value $OUTPUT_STATE_PV)"
    
    print_section "테스트용 설정 적용"
    set_pv_value "$THRESHOLD_PV" "2.5"
    set_pv_value "$HYSTERESIS_PV" "0.2"
    set_pv_value "$UPDATE_RATE_PV" "10.0"
    
    sleep 1
    
    print_success "테스트 설정 완료:"
    echo "  - 임계값: 2.5V"
    echo "  - 히스테리시스: 0.2V"
    echo "  - 업데이트 주기: 10Hz"
}

function test_enable_disable() {
    print_header "3. 활성화/비활성화 테스트"
    
    print_section "컨트롤러 비활성화"
    set_pv_value "$ENABLE_PV" "0"
    sleep 1
    local enable_state=$(get_pv_value $ENABLE_PV)
    if [ "$enable_state" = "0" ]; then
        print_success "비활성화 성공"
    else
        print_error "비활성화 실패"
    fi
    
    print_section "컨트롤러 활성화"
    set_pv_value "$ENABLE_PV" "1"
    sleep 2
    enable_state=$(get_pv_value $ENABLE_PV)
    if [ "$enable_state" = "1" ]; then
        print_success "활성화 성공"
    else
        print_error "활성화 실패"
    fi
    
    print_info "활성화 후 현재 상태:"
    echo "  - 현재값: $(get_pv_value $CURRENT_VALUE_PV) V"
    echo "  - 출력 상태: $(get_pv_value $OUTPUT_STATE_PV)"
}

function test_threshold_logic() {
    print_header "4. 임계값 로직 테스트"
    
    # 컨트롤러가 활성화되어 있는지 확인
    if [ "$(get_pv_value $ENABLE_PV)" != "1" ]; then
        print_info "컨트롤러 활성화 중..."
        set_pv_value "$ENABLE_PV" "1"
        sleep 2
    fi
    
    local test_voltages=(0.0 1.0 2.0 2.5 3.0 4.0 3.0 2.3 2.0 1.0 0.0)
    
    print_section "임계값 로직 테스트 시작"
    print_info "임계값: 2.5V, 히스테리시스: 0.2V"
    print_info "예상 동작:"
    print_info "  - 입력 > 2.5V → 출력 HIGH"
    print_info "  - 입력 < 2.3V (2.5-0.2) → 출력 LOW"
    echo
    
    for voltage in "${test_voltages[@]}"; do
        print_section "테스트 전압: ${voltage}V"
        
        # 아날로그 출력으로 입력 시뮬레이션
        set_pv_value "$AO_PV" "$voltage"
        sleep 3
        
        # 현재 상태 확인
        local current_val=$(get_pv_value $CURRENT_VALUE_PV)
        local output_state=$(get_pv_value $OUTPUT_STATE_PV)
        local output_text="LOW"
        if [ "$output_state" = "1" ]; then
            output_text="HIGH"
        fi
        
        echo "  설정값: ${voltage}V → 측정값: ${current_val}V → 출력: ${output_text}"
        
        # 로직 검증
        if (( $(echo "$voltage > 2.5" | bc -l) )); then
            if [ "$output_state" = "1" ]; then
                print_success "  로직 정상: 임계값 초과 → HIGH"
            else
                print_error "  로직 오류: 임계값 초과했지만 LOW"
            fi
        elif (( $(echo "$voltage < 2.3" | bc -l) )); then
            if [ "$output_state" = "0" ]; then
                print_success "  로직 정상: 히스테리시스 미만 → LOW"
            else
                print_error "  로직 오류: 히스테리시스 미만이지만 HIGH"
            fi
        else
            print_info "  히스테리시스 구간: 이전 상태 유지"
        fi
        
        sleep 1
    done
}

function test_hysteresis() {
    print_header "5. 히스테리시스 기능 테스트"
    
    print_section "히스테리시스 테스트 준비"
    set_pv_value "$THRESHOLD_PV" "3.0"
    set_pv_value "$HYSTERESIS_PV" "0.5"
    sleep 1
    
    print_info "설정: 임계값=3.0V, 히스테리시스=0.5V"
    print_info "예상 동작: 3.0V에서 HIGH, 2.5V에서 LOW"
    
    # 히스테리시스 테스트 시퀀스
    local hyst_voltages=(0.0 2.0 2.8 3.2 2.8 2.3 3.5 2.0)
    
    for voltage in "${hyst_voltages[@]}"; do
        print_section "히스테리시스 테스트: ${voltage}V"
        set_pv_value "$AO_PV" "$voltage"
        sleep 3
        
        local current_val=$(get_pv_value $CURRENT_VALUE_PV)
        local output_state=$(get_pv_value $OUTPUT_STATE_PV)
        local output_text="LOW"
        if [ "$output_state" = "1" ]; then
            output_text="HIGH"
        fi
        
        echo "  ${voltage}V → 측정: ${current_val}V → 출력: ${output_text}"
        sleep 1
    done
    
    # 원래 설정으로 복원
    set_pv_value "$THRESHOLD_PV" "2.5"
    set_pv_value "$HYSTERESIS_PV" "0.2"
}

function test_update_rate() {
    print_header "6. 업데이트 주기 테스트"
    
    local rates=(1.0 5.0 20.0 10.0)
    
    for rate in "${rates[@]}"; do
        print_section "업데이트 주기: ${rate}Hz"
        set_pv_value "$UPDATE_RATE_PV" "$rate"
        sleep 1
        
        local actual_rate=$(get_pv_value $UPDATE_RATE_PV)
        echo "  설정: ${rate}Hz → 실제: ${actual_rate}Hz"
        
        print_info "5초간 모니터링..."
        monitor_pvs 5
    done
}

function test_alarm_conditions() {
    print_header "7. 알람 조건 테스트"
    
    print_section "정상 범위 테스트"
    set_pv_value "$AO_PV" "2.0"
    sleep 3
    echo "알람 상태: $(get_pv_value $ALARM_STATE_PV)"
    
    print_section "범위 초과 테스트"
    set_pv_value "$AO_PV" "9.5"
    sleep 3
    echo "알람 상태: $(get_pv_value $ALARM_STATE_PV)"
    
    # 정상 범위로 복원
    set_pv_value "$AO_PV" "2.0"
    sleep 2
}

function test_reset_function() {
    print_header "8. 리셋 기능 테스트"
    
    print_section "리셋 명령 전송"
    if check_pv_connection "$RESET_PV"; then
        set_pv_value "$RESET_PV" "1"
        sleep 2
        print_success "리셋 명령 전송 완료"
        
        print_info "리셋 후 상태:"
        echo "  - 현재값: $(get_pv_value $CURRENT_VALUE_PV) V"
        echo "  - 출력 상태: $(get_pv_value $OUTPUT_STATE_PV)"
        echo "  - 알람 상태: $(get_pv_value $ALARM_STATE_PV)"
    else
        print_error "리셋 PV를 사용할 수 없습니다"
    fi
}

function test_continuous_monitoring() {
    print_header "9. 연속 모니터링 테스트"
    
    print_section "사인파 입력으로 연속 테스트"
    print_info "30초간 사인파 입력을 생성하여 ThresholdLogic 동작을 확인합니다"
    print_info "임계값: 2.5V, 진폭: 4V (0V ~ 4V)"
    
    # 백그라운드에서 모니터링 시작
    timeout 35 camonitor "$CURRENT_VALUE_PV" "$OUTPUT_STATE_PV" > /tmp/threshold_monitor.log 2>&1 &
    local monitor_pid=$!
    
    # 사인파 생성 (30초, 0.5Hz)
    local samples=60
    local duration=30
    local amplitude=2.0
    local offset=2.0
    
    for ((i = 0; i < samples; i++)); do
        local t=$(echo "$i * $duration / $samples" | bc -l)
        local theta=$(echo "2 * 3.141592 * 0.5 * $t" | bc -l)
        local sin_val=$(python3 -c "import math; print(math.sin($theta))" 2>/dev/null || echo "0")
        local voltage=$(echo "$offset + $amplitude * $sin_val" | bc -l)
        
        set_pv_value "$AO_PV" "$voltage"
        printf "시간: %2ds, 입력: %.2fV\r" "$((i/2))" "$voltage"
        sleep 0.5
    done
    
    echo
    wait $monitor_pid 2>/dev/null || true
    
    print_success "연속 모니터링 테스트 완료"
    if [ -f /tmp/threshold_monitor.log ]; then
        local transitions=$(grep -c "High\|Low" /tmp/threshold_monitor.log 2>/dev/null || echo "0")
        print_info "출력 상태 변화 횟수: $transitions"
        rm -f /tmp/threshold_monitor.log
    fi
}

function test_performance() {
    print_header "10. 성능 테스트"
    
    print_section "빠른 변화 테스트"
    set_pv_value "$UPDATE_RATE_PV" "50.0"
    sleep 1
    
    print_info "빠른 전압 변화로 응답성 테스트 (50Hz 업데이트)"
    local fast_voltages=(1.0 3.0 1.0 3.0 1.0 3.0 1.0 3.0)
    
    for voltage in "${fast_voltages[@]}"; do
        set_pv_value "$AO_PV" "$voltage"
        sleep 0.2
        local output=$(get_pv_value $OUTPUT_STATE_PV)
        printf "%.1fV → %s  " "$voltage" "$output"
    done
    echo
    
    # 원래 설정으로 복원
    set_pv_value "$UPDATE_RATE_PV" "10.0"
    set_pv_value "$AO_PV" "0.0"
}

function generate_test_report() {
    print_header "테스트 결과 요약"
    
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    local report_file="/tmp/threshold_logic_test_report.txt"
    
    {
        echo "ThresholdLogic Controller 테스트 보고서"
        echo "생성 시간: $timestamp"
        echo "========================================"
        echo
        echo "최종 설정 상태:"
        echo "  임계값: $(get_pv_value $THRESHOLD_PV) V"
        echo "  히스테리시스: $(get_pv_value $HYSTERESIS_PV) V"
        echo "  업데이트 주기: $(get_pv_value $UPDATE_RATE_PV) Hz"
        echo "  활성화 상태: $(get_pv_value $ENABLE_PV)"
        echo
        echo "최종 동작 상태:"
        echo "  현재값: $(get_pv_value $CURRENT_VALUE_PV) V"
        echo "  출력 상태: $(get_pv_value $OUTPUT_STATE_PV)"
        echo "  알람 상태: $(get_pv_value $ALARM_STATE_PV)"
        echo
        echo "테스트 완료 항목:"
        echo "  ✓ PV 연결 테스트"
        echo "  ✓ 초기 설정 테스트"
        echo "  ✓ 활성화/비활성화 테스트"
        echo "  ✓ 임계값 로직 테스트"
        echo "  ✓ 히스테리시스 기능 테스트"
        echo "  ✓ 업데이트 주기 테스트"
        echo "  ✓ 알람 조건 테스트"
        echo "  ✓ 연속 모니터링 테스트"
        echo "  ✓ 성능 테스트"
    } > "$report_file"
    
    cat "$report_file"
    print_success "테스트 보고서가 생성되었습니다: $report_file"
}

function cleanup() {
    print_header "정리 작업"
    
    print_section "설정 초기화"
    set_pv_value "$AO_PV" "0.0"
    set_pv_value "$THRESHOLD_PV" "2.5"
    set_pv_value "$HYSTERESIS_PV" "0.1"
    set_pv_value "$UPDATE_RATE_PV" "10.0"
    set_pv_value "$ENABLE_PV" "0"
    
    print_success "정리 작업 완료"
}

# ===== 메인 함수 =====

function show_menu() {
    echo -e "\n${CYAN}ThresholdLogic Controller 테스트 메뉴${NC}"
    echo "1) 전체 테스트 실행"
    echo "2) PV 연결 테스트만"
    echo "3) 임계값 로직 테스트만"
    echo "4) 히스테리시스 테스트만"
    echo "5) 연속 모니터링 테스트만"
    echo "6) 실시간 모니터링 (Ctrl+C로 중지)"
    echo "7) 현재 상태 확인"
    echo "8) 설정 초기화"
    echo "9) 종료"
    echo -n "선택하세요 (1-9): "
}

function interactive_mode() {
    while true; do
        show_menu
        read choice
        
        case $choice in
            1)
                run_all_tests
                ;;
            2)
                test_pv_connections
                ;;
            3)
                test_threshold_logic
                ;;
            4)
                test_hysteresis
                ;;
            5)
                test_continuous_monitoring
                ;;
            6)
                print_info "실시간 모니터링 시작 (Ctrl+C로 중지)..."
                camonitor "$CURRENT_VALUE_PV" "$OUTPUT_STATE_PV" "$THRESHOLD_PV" "$ENABLE_PV"
                ;;
            7)
                print_header "현재 상태"
                echo "임계값: $(get_pv_value $THRESHOLD_PV) V"
                echo "히스테리시스: $(get_pv_value $HYSTERESIS_PV) V"
                echo "현재값: $(get_pv_value $CURRENT_VALUE_PV) V"
                echo "출력 상태: $(get_pv_value $OUTPUT_STATE_PV)"
                echo "활성화 상태: $(get_pv_value $ENABLE_PV)"
                echo "업데이트 주기: $(get_pv_value $UPDATE_RATE_PV) Hz"
                ;;
            8)
                cleanup
                ;;
            9)
                print_info "테스트를 종료합니다."
                exit 0
                ;;
            *)
                print_error "잘못된 선택입니다."
                ;;
        esac
        
        wait_for_user
    done
}

function run_all_tests() {
    print_header "ThresholdLogic Controller 전체 테스트 시작"
    
    test_pv_connections
    test_initial_setup
    test_enable_disable
    test_threshold_logic
    test_hysteresis
    test_update_rate
    test_alarm_conditions
    test_reset_function
    test_continuous_monitoring
    test_performance
    generate_test_report
    cleanup
    
    print_header "모든 테스트 완료!"
}

# ===== 스크립트 시작 =====

function main() {
    print_header "ThresholdLogic Controller 테스트 스크립트"
    print_info "이 스크립트는 ThresholdLogicController의 모든 기능을 테스트합니다."
    print_info "IOC가 실행 중이고 ThresholdLogic이 설정되어 있는지 확인하세요."
    echo
    
    # 명령행 인수 확인
    if [ "$1" = "--auto" ] || [ "$1" = "-a" ]; then
        print_info "자동 모드로 전체 테스트를 실행합니다..."
        run_all_tests
    else
        print_info "대화형 모드로 시작합니다..."
        interactive_mode
    fi
}

# 스크립트 실행
main "$@"