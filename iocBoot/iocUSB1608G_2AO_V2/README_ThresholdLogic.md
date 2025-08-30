# ThresholdLogic Controller MEDM 및 테스트 가이드

## 개요

이 문서는 USB1608G-2AO IOC에 추가된 ThresholdLogicController의 MEDM 화면과 테스트 스크립트 사용법을 설명합니다.

## 파일 구성

### MEDM 화면
- `USB1608G_2AO_cpp.adl` - 기존 화면에 ThresholdLogic 섹션 추가
- `medm_ThresholdLogic.sh` - ThresholdLogic 전용 MEDM 실행 스크립트
- `medm_USB1608G_2AO.sh` - 기존 스크립트에 ThresholdLogic 옵션 추가

### 테스트 스크립트
- `catest_ThresholdLogic.sh` - 포괄적인 ThresholdLogic 테스트 스크립트
- `catest_USB1608G_2AO.sh` - 기존 USB1608G-2AO 테스트 스크립트

## MEDM 화면 사용법

### 1. 전체 시스템 MEDM 화면 실행

```bash
# 방법 1: 기존 스크립트 사용 (메뉴에서 선택)
./medm_USB1608G_2AO.sh
# 메뉴에서 1번 또는 2번 선택

# 방법 2: 직접 실행
./medm_ThresholdLogic.sh
```

### 2. MEDM 화면 구성

#### 기존 섹션 (상단)
- **장치 정보**: 모델명, 고유 ID, 드라이버 버전
- **디지털 I/O**: 8채널 디지털 입출력 제어
- **아날로그 입력**: 8채널 아날로그 입력 모니터링
- **아날로그 출력**: 2채널 아날로그 출력 제어
- **트렌드 차트**: 실시간 데이터 시각화

#### 새로 추가된 ThresholdLogic 섹션 (하단)

**설정 컨트롤 (좌측)**:
- **Threshold**: 임계값 설정 (V 단위)
- **Hysteresis**: 히스테리시스 값 설정 (V 단위)
- **Update Rate**: 업데이트 주기 설정 (Hz 단위)
- **Logic Status**: 현재 로직 상태 표시
- **Reset**: 시스템 리셋 버튼

**상태 모니터링 (중앙)**:
- **Current Value**: 현재 입력값 실시간 표시
- **Output State**: 출력 상태 (LED 표시)
- **Enable**: 컨트롤러 활성화/비활성화 버튼
- **Alarm Status**: 알람 상태 표시

**시각화 (우측)**:
- **Meter**: 현재값을 아날로그 미터로 표시
- **Trend Chart**: 현재값과 임계값의 시간 추이

**로직 설명 박스**:
- 임계값 로직의 동작 원리 설명
- 히스테리시스 기능 설명

### 3. 사용 절차

1. **IOC 시작 확인**
   ```bash
   # IOC가 실행 중인지 확인
   ps aux | grep USB1608G_2AO_cpp
   ```

2. **MEDM 화면 실행**
   ```bash
   ./medm_ThresholdLogic.sh
   ```

3. **ThresholdLogic 설정**
   - Threshold 필드에 원하는 임계값 입력 (예: 2.5)
   - Hysteresis 필드에 히스테리시스 값 입력 (예: 0.1)
   - Update Rate 설정 (기본값: 10Hz)

4. **컨트롤러 활성화**
   - Enable 버튼을 클릭하여 "Enabled"로 변경

5. **동작 확인**
   - Current Value가 실시간으로 업데이트되는지 확인
   - 임계값을 초과하면 Output State가 "High"로 변경되는지 확인
   - 트렌드 차트에서 시간에 따른 변화 관찰

## 테스트 스크립트 사용법

### 1. 자동 전체 테스트

```bash
# 모든 테스트를 자동으로 실행
./catest_ThresholdLogic.sh --auto
```

### 2. 대화형 테스트

```bash
# 대화형 메뉴로 실행
./catest_ThresholdLogic.sh
```

메뉴 옵션:
1. **전체 테스트 실행** - 모든 테스트를 순차적으로 실행
2. **PV 연결 테스트만** - PV 연결 상태만 확인
3. **임계값 로직 테스트만** - 임계값 비교 로직만 테스트
4. **히스테리시스 테스트만** - 히스테리시스 기능만 테스트
5. **연속 모니터링 테스트만** - 사인파 입력으로 연속 테스트
6. **실시간 모니터링** - 실시간 PV 값 모니터링
7. **현재 상태 확인** - 현재 설정 및 상태 확인
8. **설정 초기화** - 모든 설정을 기본값으로 초기화
9. **종료** - 스크립트 종료

### 3. 테스트 내용

#### PV 연결 테스트
- 모든 ThresholdLogic PV의 연결 상태 확인
- IOC 실행 상태 검증

#### 임계값 로직 테스트
- 다양한 입력 전압에 대한 출력 상태 확인
- 임계값 초과/미만 시 동작 검증
- 로직 정확성 검증

#### 히스테리시스 테스트
- 히스테리시스 기능 동작 확인
- 출력 진동 방지 기능 검증
- 상향/하향 임계값 동작 확인

#### 연속 모니터링 테스트
- 사인파 입력으로 30초간 연속 테스트
- 출력 상태 변화 횟수 카운트
- 실시간 응답성 확인

#### 성능 테스트
- 빠른 입력 변화에 대한 응답성 테스트
- 다양한 업데이트 주기에서의 동작 확인
- CPU 사용률 및 안정성 검증

## 문제 해결

### 1. MEDM 실행 오류

**증상**: MEDM이 시작되지 않음
```bash
medm: command not found
```

**해결 방법**:
```bash
# MEDM 설치 확인
which medm

# EPICS Extensions 경로 확인
echo $PATH | grep -i medm

# 필요시 PATH 추가
export PATH=$PATH:/usr/local/epics/extensions/bin/$EPICS_HOST_ARCH
```

### 2. PV 연결 실패

**증상**: PV 연결 타임아웃
```bash
Channel connect timed out
```

**해결 방법**:
```bash
# IOC 실행 상태 확인
ps aux | grep USB1608G_2AO_cpp

# IOC 재시작
cd iocBoot/iocUSB1608G_2AO_cpp
../../bin/linux-x86_64/USB1608G_2AO_cpp st.cmd

# 네트워크 설정 확인
echo $EPICS_CA_ADDR_LIST
echo $EPICS_CA_AUTO_ADDR_LIST
```

### 3. ThresholdLogic 동작 안함

**증상**: 임계값을 초과해도 출력이 변경되지 않음

**해결 방법**:
```bash
# 컨트롤러 활성화 상태 확인
caget USB1608G_2AO_cpp:ThresholdLogic1Enable

# 활성화
caput USB1608G_2AO_cpp:ThresholdLogic1Enable 1

# 현재값 확인
caget USB1608G_2AO_cpp:ThresholdLogic1CurrentValue

# 임계값 확인
caget USB1608G_2AO_cpp:ThresholdLogic1Threshold
```

### 4. 테스트 스크립트 권한 오류

**증상**: Permission denied

**해결 방법**:
```bash
# 실행 권한 부여
chmod +x catest_ThresholdLogic.sh
chmod +x medm_ThresholdLogic.sh

# 스크립트 실행
./catest_ThresholdLogic.sh
```

## 고급 사용법

### 1. 사용자 정의 테스트

테스트 스크립트를 수정하여 특정 요구사항에 맞는 테스트 추가:

```bash
# 스크립트 복사
cp catest_ThresholdLogic.sh my_custom_test.sh

# 사용자 정의 테스트 함수 추가
function my_custom_test() {
    print_header "사용자 정의 테스트"
    # 여기에 사용자 정의 테스트 로직 추가
}
```

### 2. 자동화된 모니터링

시스템 모니터링을 위한 자동화 스크립트:

```bash
# 주기적 상태 확인
while true; do
    echo "$(date): $(caget -t USB1608G_2AO_cpp:ThresholdLogic1CurrentValue) V"
    sleep 10
done
```

### 3. 로그 분석

테스트 결과 로그 분석:

```bash
# 테스트 보고서 확인
cat /tmp/threshold_logic_test_report.txt

# 특정 패턴 검색
grep -i "error\|fail" /tmp/threshold_logic_test_report.txt
```

## 참고 자료

- [EPICS MEDM 사용자 가이드](https://epics.anl.gov/EpicsDocumentation/ExtensionsManuals/MEDM/MEDM.html)
- [Channel Access 클라이언트 도구](https://epics.anl.gov/base/R7-0/8-docs/CAref.html)
- [ThresholdLogicController API 문서](../../docs/ThresholdLogicController_API_Documentation.md)
- [문제 해결 가이드](../../docs/ThresholdLogicController_Troubleshooting_Guide.md)

## 버전 정보

- 생성일: 2025년 8월 20일
- ThresholdLogicController 버전: 1.0.0
- EPICS Base 호환성: R7.0+
- 테스트 환경: Linux x86_64