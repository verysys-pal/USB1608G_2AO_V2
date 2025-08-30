/* ThresholdLogicController.cpp
 *
 * 임계값 기반 로직 제어를 위한 asynPortDriver 구현
 * 
 * 이 드라이버는 아날로그 입력 값을 모니터링하고 설정된 임계값과 비교하여
 * 디지털 출력을 제어하는 기능을 제공합니다.
 * 히스테리시스 기능을 포함하여 안정적인 출력 제어를 보장합니다.
 *
 * Author: EPICS IOC Development Guide
 * Date: 2025
 */


/* ThresholdLogicController 구현된 내용:         */
// 1. ThresholdLogicController.cpp 파일 생성
// 2. asynPortDriver 초기화 및 매개변수 생성
//     생성자에서 asynPortDriver를 올바르게 초기화
//     9개의 매개변수를 생성 (임계값, 현재값, 출력상태, 활성화, 히스테리시스 등)
//     초기값 설정 및 매개변수 콜백 호출
// 3. 기본적인 읽기/쓰기 메서드 스켈레톤 구현
//     writeFloat64(): 임계값, 히스테리시스, 업데이트 주기 설정 처리
//     readFloat64(): 현재값 읽기 및 캐시된 값 반환
//     writeInt32(): 활성화 상태 제어 및 출력상태 보호
//     readInt32(): 정수 매개변수 읽기
// 4. 추가 구현된 기능들
//     소멸자: 리소스 정리 및 스레드 중지
//     IOC 쉘 명령어: ThresholdLogicConfig 함수 및 등록
//     내부 메서드 스켈레톤: 향후 작업에서 구현할 메서드들의 기본 틀
//     오류 처리: 매개변수 유효성 검사 및 로깅
//     한국어 주석: 모든 주요 기능에 대한 한국어 설명
// 5. 요구사항 충족 확인
//     요구사항 4.1: asynPortDriver 상속 및 매개변수 관리 구현
//     요구사항 4.2: 기본 구조 및 인터페이스 정의


#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <iocsh.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <asynPortDriver.h>
#include <asynOctetSyncIO.h>

#include "ThresholdLogicController.h"
#include "ErrorHandler.h"

static const char *driverName = "ThresholdLogicController";

/** ThresholdLogicController 생성자
 * \param[in] portName 이 드라이버의 asyn 포트 이름
 * \param[in] devicePort 연결할 장치 포트 이름  
 * \param[in] deviceAddr 장치 주소
 */
ThresholdLogicController::ThresholdLogicController(const char* portName, const char* devicePort, int deviceAddr)
    : asynPortDriver(portName, 
                     1, /* maxAddr */ 
                     asynFloat64Mask | asynInt32Mask | asynDrvUserMask, /* Interface mask */
                     asynFloat64Mask | asynInt32Mask,  /* Interrupt mask */
                     ASYN_CANBLOCK, /* asynFlags */
                     1, /* Autoconnect */
                     0, /* Default priority */
                     0) /* Default stack size */
{
    const char* functionName = "ThresholdLogicController";
    
    // 장치 연결 정보 저장
    strncpy(devicePortName_, devicePort, sizeof(devicePortName_) - 1);
    devicePortName_[sizeof(devicePortName_) - 1] = '\0';
    deviceAddr_ = deviceAddr;
    
    // 매개변수 생성
    createParam(THRESHOLD_VALUE_STRING,  asynParamFloat64, &P_ThresholdValue);
    createParam(CURRENT_VALUE_STRING,    asynParamFloat64, &P_CurrentValue);
    createParam(OUTPUT_STATE_STRING,     asynParamInt32,   &P_OutputState);
    createParam(COMPARE_RESULT_STRING,   asynParamInt32,   &P_CompareResult);
    createParam(ENABLE_STRING,           asynParamInt32,   &P_Enable);
    createParam(HYSTERESIS_STRING,       asynParamFloat64, &P_Hysteresis);
    createParam(UPDATE_RATE_STRING,      asynParamFloat64, &P_UpdateRate);
    createParam(ALARM_STATUS_STRING,     asynParamInt32,   &P_AlarmStatus);
    createParam(DEVICE_PORT_STRING,      asynParamOctet,   &P_DevicePort);
    createParam(DEVICE_ADDR_STRING,      asynParamInt32,   &P_DeviceAddr);
    
    // 초기값 설정
    thresholdValue_ = 0.0;
    currentValue_ = 0.0;
    outputState_ = false;
    enabled_ = false;
    hysteresis_ = 0.1;  // 기본 히스테리시스 값
    updateRate_ = 10.0; // 기본 10Hz 업데이트
    alarmStatus_ = 0;   // 알람 없음
    lastOutputState_ = false;
    
    // 스레드 관리 변수 초기화
    monitorThread_ = NULL;
    threadRunning_ = false;
    threadExit_ = false;
    
    // 매개변수 초기값을 데이터베이스에 설정
    setDoubleParam(P_ThresholdValue, thresholdValue_);
    setDoubleParam(P_CurrentValue, currentValue_);
    setIntegerParam(P_OutputState, outputState_ ? 1 : 0);
    setIntegerParam(P_CompareResult, outputState_ ? 1 : 0);
    setIntegerParam(P_Enable, enabled_ ? 1 : 0);
    setDoubleParam(P_Hysteresis, hysteresis_);
    setDoubleParam(P_UpdateRate, updateRate_);
    setIntegerParam(P_AlarmStatus, alarmStatus_);
    setStringParam(P_DevicePort, devicePortName_);
    setIntegerParam(P_DeviceAddr, deviceAddr_);
    
    // 타임스탬프 초기화
    epicsTimeGetCurrent(&lastUpdate_);
    
    // 구성 유효성 검사 (ErrorHandler 사용)
    if (!validateConfigurationWithErrorHandler()) {
        ErrorHandler::logError(ErrorHandler::WARNING, functionName, 
                              "구성 유효성 검사에서 경고가 발생했습니다", pasynUserSelf);
    }
    
    // 매개변수 변경사항을 클라이언트에 알림
    callParamCallbacks();
    
    // 성공적인 생성 로그
    char successMessage[256];
    snprintf(successMessage, sizeof(successMessage),
            "포트=%s, 장치포트=%s, 주소=%d로 ThresholdLogicController 생성됨",
            portName, devicePort, deviceAddr);
    ErrorHandler::logError(ErrorHandler::INFO, functionName, successMessage, pasynUserSelf);
}

/** ThresholdLogicController 소멸자 */
ThresholdLogicController::~ThresholdLogicController()
{
    const char* functionName = "~ThresholdLogicController";
    
    // 모니터링 스레드 중지
    stopMonitoring();
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: ThresholdLogicController 소멸됨\n",
              driverName, functionName);
}

/** Float64 매개변수 쓰기 메서드 */
asynStatus ThresholdLogicController::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char* functionName = "writeFloat64";
    
    // 매개변수별 유효성 검사 및 처리
    if (function == P_ThresholdValue) {
        // 임계값 유효성 검사 (ErrorHandler 사용)
        if (!ErrorHandler::validateParameter("thresholdValue", value, -10.0, 10.0, functionName)) {
            ErrorHandler::logError(ErrorHandler::ERROR, functionName, 
                                  "임계값이 유효 범위(-10.0V ~ +10.0V)를 벗어났습니다", pasynUser);
            return asynError;
        }
        
        // 히스테리시스와의 관계 검사
        if (fabs(value) < hysteresis_) {
            char warningMsg[256];
            snprintf(warningMsg, sizeof(warningMsg),
                    "임계값이 히스테리시스보다 작음 - 임계값: %f, 히스테리시스: %f",
                    value, hysteresis_);
            ErrorHandler::logError(ErrorHandler::WARNING, functionName, warningMsg, pasynUser);
        }
        
        thresholdValue_ = value;
        status = setDoubleParam(function, value);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s::%s: 임계값 설정됨: %f V\n", driverName, functionName, value);
    }
    else if (function == P_Hysteresis) {
        // 히스테리시스 유효성 검사 (ErrorHandler 사용)
        if (!ErrorHandler::validateParameter("hysteresis", value, 0.0, 5.0, functionName)) {
            ErrorHandler::logError(ErrorHandler::ERROR, functionName, 
                                  "히스테리시스가 유효 범위(0.0V ~ 5.0V)를 벗어났습니다", pasynUser);
            return asynError;
        }
        
        // 임계값과의 관계 검사
        if (value > fabs(thresholdValue_)) {
            asynPrint(pasynUser, ASYN_TRACE_WARNING,
                      "%s::%s: 히스테리시스가 임계값보다 큼 - 히스테리시스: %f, 임계값: %f\n",
                      driverName, functionName, value, thresholdValue_);
        }
        
        hysteresis_ = value;
        status = setDoubleParam(function, value);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s::%s: 히스테리시스 설정됨: %f V\n", driverName, functionName, value);
    }
    else if (function == P_UpdateRate) {
        // 업데이트 주기 유효성 검사 (ErrorHandler 사용)
        if (!ErrorHandler::validateParameter("updateRate", value, 0.1, 1000.0, functionName)) {
            ErrorHandler::logError(ErrorHandler::ERROR, functionName, 
                                  "업데이트 주기가 유효 범위(0.1Hz ~ 1000Hz)를 벗어났습니다", pasynUser);
            return asynError;
        }
        
        double oldRate = updateRate_;
        updateRate_ = value;
        status = setDoubleParam(function, value);
        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s::%s: 업데이트 주기 변경됨: %f Hz -> %f Hz\n", 
                  driverName, functionName, oldRate, value);
        
        // 스레드가 실행 중인 경우 새로운 주기가 다음 루프에서 적용됨을 알림
        if (threadRunning_) {
            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                      "%s::%s: 새로운 업데이트 주기는 다음 루프에서 적용됩니다\n",
                      driverName, functionName);
        }
    }
    else if (function == P_CurrentValue) {
        // 현재값은 읽기 전용이므로 쓰기 거부
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 현재값은 읽기 전용 매개변수입니다\n",
                  driverName, functionName);
        return asynError;
    }
    else {
        // 알 수 없는 매개변수에 대해서는 부모 클래스 호출
        asynPrint(pasynUser, ASYN_TRACE_WARNING,
                  "%s::%s: 알 수 없는 Float64 매개변수: function=%d\n",
                  driverName, functionName, function);
        status = asynPortDriver::writeFloat64(pasynUser, value);
    }
    
    // 성공한 경우에만 매개변수 변경사항을 클라이언트에 알림
    if (status == asynSuccess) {
        callParamCallbacks();
    } else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 매개변수 설정 실패 - function=%d, value=%f, status=%d\n",
                  driverName, functionName, function, value, status);
    }
    
    return status;
}

/** Float64 매개변수 읽기 메서드 */
asynStatus ThresholdLogicController::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char* functionName = "readFloat64";
    
    // 입력 매개변수 유효성 검사
    if (value == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: NULL 포인터가 전달됨\n",
                  driverName, functionName);
        return asynError;
    }
    
    // 매개변수별 읽기 처리
    if (function == P_ThresholdValue) {
        *value = thresholdValue_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 임계값 읽기: %f V\n", driverName, functionName, *value);
    }
    else if (function == P_CurrentValue) {
        // 현재값은 실시간으로 업데이트되므로 최신 값 반환
        *value = currentValue_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 현재값 읽기: %f V\n", driverName, functionName, *value);
        
        // 현재값이 유효 범위를 벗어나는 경우 경고
        if (*value < -10.0 || *value > 10.0) {
            asynPrint(pasynUser, ASYN_TRACE_WARNING,
                      "%s::%s: 현재값이 예상 범위를 벗어남: %f V\n",
                      driverName, functionName, *value);
        }
    }
    else if (function == P_Hysteresis) {
        *value = hysteresis_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 히스테리시스 읽기: %f V\n", driverName, functionName, *value);
    }
    else if (function == P_UpdateRate) {
        *value = updateRate_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 업데이트 주기 읽기: %f Hz\n", driverName, functionName, *value);
    }
    else {
        // 알 수 없는 매개변수에 대해서는 부모 클래스 호출
        asynPrint(pasynUser, ASYN_TRACE_WARNING,
                  "%s::%s: 알 수 없는 Float64 매개변수: function=%d\n",
                  driverName, functionName, function);
        status = asynPortDriver::readFloat64(pasynUser, value);
        
        if (status != asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s::%s: 부모 클래스에서 매개변수 읽기 실패: function=%d\n",
                      driverName, functionName, function);
        }
    }
    
    // 오류 발생 시 로깅
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 매개변수 읽기 실패 - function=%d, status=%d\n",
                  driverName, functionName, function, status);
    }
    
    return status;
}

/** Int32 매개변수 쓰기 메서드 */
asynStatus ThresholdLogicController::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char* functionName = "writeInt32";
    
    // 매개변수별 유효성 검사 및 처리
    if (function == P_Enable) {
        // 활성화 상태 유효성 검사 (0 또는 1만 허용)
        if (value != 0 && value != 1) {
            asynPrint(pasynUser, ASYN_TRACE_WARNING,
                      "%s::%s: 활성화 값이 0 또는 1이 아님: %d (0으로 처리)\n",
                      driverName, functionName, value);
            value = (value != 0) ? 1 : 0; // 0이 아닌 값은 1로 처리
        }
        
        bool newEnabled = (value != 0);
        if (newEnabled != enabled_) {
            // 상태 변경 전 유효성 검사
            if (newEnabled) {
                // 활성화하기 전 필수 매개변수 검사
                if (!validateParameters()) {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                              "%s::%s: 매개변수 유효성 검사 실패 - 활성화할 수 없음\n",
                              driverName, functionName);
                    return asynError;
                }
                
                // 장치 연결 상태 확인
                if (strlen(devicePortName_) == 0) {
                    asynPrint(pasynUser, ASYN_TRACE_ERROR,
                              "%s::%s: 장치 포트가 설정되지 않음 - 활성화할 수 없음\n",
                              driverName, functionName);
                    return asynError;
                }
            }
            
            enabled_ = newEnabled;
            status = setIntegerParam(function, value);
            
            if (enabled_) {
                startMonitoring();
                asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                          "%s::%s: 임계값 로직 활성화됨 (포트: %s, 주소: %d)\n", 
                          driverName, functionName, devicePortName_, deviceAddr_);
            } else {
                stopMonitoring();
                asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                          "%s::%s: 임계값 로직 비활성화됨\n", driverName, functionName);
            }
        } else {
            // 상태 변경이 없는 경우에도 매개변수 업데이트
            status = setIntegerParam(function, value);
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                      "%s::%s: 활성화 상태 유지: %s\n", 
                      driverName, functionName, enabled_ ? "활성화" : "비활성화");
        }
    }
    else if (function == P_OutputState) {
        // 출력 상태는 읽기 전용이므로 쓰기 거부
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 출력 상태는 읽기 전용 매개변수입니다 (시도된 값: %d)\n",
                  driverName, functionName, value);
        return asynError;
    }
    else if (function == P_AlarmStatus) {
        // 알람 상태도 읽기 전용이므로 쓰기 거부
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 알람 상태는 읽기 전용 매개변수입니다 (시도된 값: %d)\n",
                  driverName, functionName, value);
        return asynError;
    }
    else if (function == P_DeviceAddr) {
        // 장치 주소 유효성 검사 (0-255 범위)
        if (value < 0 || value > 255) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s::%s: 장치 주소가 유효 범위를 벗어남: %d (범위: 0-255)\n",
                      driverName, functionName, value);
            return asynError;
        }
        
        // 활성화 상태에서는 장치 주소 변경 불가
        if (enabled_) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s::%s: 활성화 상태에서는 장치 주소를 변경할 수 없습니다\n",
                      driverName, functionName);
            return asynError;
        }
        
        deviceAddr_ = value;
        status = setIntegerParam(function, value);
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                  "%s::%s: 장치 주소 설정됨: %d\n", driverName, functionName, value);
    }
    else {
        // 알 수 없는 매개변수에 대해서는 부모 클래스 호출
        asynPrint(pasynUser, ASYN_TRACE_WARNING,
                  "%s::%s: 알 수 없는 Int32 매개변수: function=%d, value=%d\n",
                  driverName, functionName, function, value);
        status = asynPortDriver::writeInt32(pasynUser, value);
    }
    
    // 성공한 경우에만 매개변수 변경사항을 클라이언트에 알림
    if (status == asynSuccess) {
        callParamCallbacks();
    } else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 매개변수 설정 실패 - function=%d, value=%d, status=%d\n",
                  driverName, functionName, function, value, status);
    }
    
    return status;
}

/** Int32 매개변수 읽기 메서드 */
asynStatus ThresholdLogicController::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char* functionName = "readInt32";
    
    // 입력 매개변수 유효성 검사
    if (value == NULL) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: NULL 포인터가 전달됨\n",
                  driverName, functionName);
        return asynError;
    }
    
    // 매개변수별 읽기 처리
    if (function == P_Enable) {
        *value = enabled_ ? 1 : 0;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 활성화 상태 읽기: %d (%s)\n", 
                  driverName, functionName, *value, enabled_ ? "활성화" : "비활성화");
    }
    else if (function == P_OutputState) {
        *value = outputState_ ? 1 : 0;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 출력 상태 읽기: %d (%s)\n",
                  driverName, functionName, *value, outputState_ ? "HIGH" : "LOW");
        
        // 스레드가 실행 중이지 않은데 출력 상태가 변경된 경우 경고
        if (!threadRunning_ && outputState_) {
            asynPrint(pasynUser, ASYN_TRACE_WARNING,
                      "%s::%s: 모니터링 스레드가 중지된 상태에서 출력이 HIGH입니다\n",
                      driverName, functionName);
        }
    }
    else if (function == P_CompareResult) {
        *value = outputState_ ? 1 : 0;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 비교 결과 읽기: %d\n",
                  driverName, functionName, *value);
    }
    else if (function == P_AlarmStatus) {
        *value = alarmStatus_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 알람 상태 읽기: %d\n", driverName, functionName, *value);
        
        // 알람 상태에 따른 추가 정보 제공
        const char* alarmDesc = "";
        switch (alarmStatus_) {
            case 0: alarmDesc = "정상"; break;
            case 1: alarmDesc = "경고"; break;
            case 2: alarmDesc = "주요 오류"; break;
            case 3: alarmDesc = "치명적 오류"; break;
            default: alarmDesc = "알 수 없음"; break;
        }
        
        if (alarmStatus_ != 0) {
            asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                      "%s::%s: 알람 상태 상세: %s\n", 
                      driverName, functionName, alarmDesc);
        }
    }
    else if (function == P_DeviceAddr) {
        *value = deviceAddr_;
        asynPrint(pasynUser, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 장치 주소 읽기: %d\n", driverName, functionName, *value);
    }
    else {
        // 알 수 없는 매개변수에 대해서는 부모 클래스 호출
        asynPrint(pasynUser, ASYN_TRACE_WARNING,
                  "%s::%s: 알 수 없는 Int32 매개변수: function=%d\n",
                  driverName, functionName, function);
        status = asynPortDriver::readInt32(pasynUser, value);
        
        if (status != asynSuccess) {
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "%s::%s: 부모 클래스에서 매개변수 읽기 실패: function=%d\n",
                      driverName, functionName, function);
        }
    }
    
    // 오류 발생 시 로깅
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "%s::%s: 매개변수 읽기 실패 - function=%d, status=%d\n",
                  driverName, functionName, function, status);
    }
    
    return status;
}

/** 임계값 로직 처리 메서드 
 * 
 * 이 메서드는 다음 기능들을 수행합니다:
 * 1. 장치에서 현재 값을 읽어옴
 * 2. 임계값과 히스테리시스를 고려한 비교 로직 수행
 * 3. 출력 상태 변화 감지 및 제어
 * 4. 알람 상태 설정 및 타임스탬프 업데이트
 */
void ThresholdLogicController::processThresholdLogic()
{
    const char* functionName = "processThresholdLogic";
    asynStatus status = asynSuccess;
    
    // 활성화되지 않은 경우 처리하지 않음
    if (!enabled_) {
        return;
    }
    
    // 1. 장치에서 현재 값을 읽어옴
    status = readCurrentValueFromDevice();
    if (status != asynSuccess) {
        ErrorHandler::handleCommunicationError(functionName, devicePortName_, deviceAddr_, 
                                              "현재값 읽기", pasynUserSelf);
        alarmStatus_ = 2; // MAJOR 알람
        ErrorHandler::setAlarmStatus(pasynUserSelf, ErrorHandler::COMM_ALARM, 
                                   ErrorHandler::MAJOR_ALARM);
        updateAlarmStatus();
        return;
    }
    
    // 2. 임계값 비교 및 히스테리시스 로직 구현
    bool newOutputState = outputState_; // 현재 출력 상태로 초기화
    
    if (!outputState_) {
        // 현재 출력이 LOW인 경우: 임계값을 초과하면 HIGH로 변경
        if (currentValue_ > thresholdValue_) {
            newOutputState = true;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                      "%s::%s: 임계값 초과 감지 - 현재값: %f, 임계값: %f\n",
                      driverName, functionName, currentValue_, thresholdValue_);
        }
    } else {
        // 현재 출력이 HIGH인 경우: 임계값-히스테리시스 아래로 떨어지면 LOW로 변경
        double lowerThreshold = thresholdValue_ - hysteresis_;
        if (currentValue_ < lowerThreshold) {
            newOutputState = false;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                      "%s::%s: 히스테리시스 임계값 미만 감지 - 현재값: %f, 하한임계값: %f\n",
                      driverName, functionName, currentValue_, lowerThreshold);
        }
    }
    
    // 3. 상태 변화 감지 및 출력 제어
    if (newOutputState != outputState_) {
        // 상태가 변경된 경우
        lastOutputState_ = outputState_; // 이전 상태 저장
        outputState_ = newOutputState;   // 새로운 상태 설정
        
        // 장치에 새로운 출력 상태 설정
        status = writeOutputStateToDevice(outputState_);
        if (status != asynSuccess) {
            ErrorHandler::handleCommunicationError(functionName, devicePortName_, deviceAddr_, 
                                                  "출력상태 설정", pasynUserSelf);
            alarmStatus_ = 2; // MAJOR 알람
            ErrorHandler::setAlarmStatus(pasynUserSelf, ErrorHandler::WRITE_ALARM, 
                                       ErrorHandler::MAJOR_ALARM);
        } else {
            // 성공적으로 출력 상태가 변경됨
            alarmStatus_ = 0; // 알람 해제
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                      "%s::%s: 출력 상태 변경됨: %s -> %s\n",
                      driverName, functionName, 
                      lastOutputState_ ? "HIGH" : "LOW",
                      outputState_ ? "HIGH" : "LOW");
        }
        
        // 출력 상태 매개변수 업데이트
        setIntegerParam(P_OutputState, outputState_ ? 1 : 0);
    } else {
        // 상태 변화가 없는 경우 - 정상 동작
        if (alarmStatus_ == 0) {
            // 이미 알람이 없는 상태라면 그대로 유지
        } else {
            // 이전에 알람이 있었다면 해제
            alarmStatus_ = 0;
            asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                      "%s::%s: 정상 동작 - 알람 해제\n",
                      driverName, functionName);
        }
    }
    
    // 4. 매개변수 업데이트 및 타임스탬프 갱신
    setDoubleParam(P_CurrentValue, currentValue_);
    setIntegerParam(P_AlarmStatus, alarmStatus_);
    setIntegerParam(P_CompareResult, outputState_ ? 1 : 0);
    
    // 타임스탬프 업데이트
    epicsTimeGetCurrent(&lastUpdate_);
    
    // 5. 알람 상태 업데이트 및 클라이언트 알림
    updateAlarmStatus();
    callParamCallbacks();
    
    // 디버그 정보 출력 (TRACE_FLOW 레벨)
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 처리 완료 - 현재값: %f, 임계값: %f, 출력: %s, 알람: %d\n",
              driverName, functionName, currentValue_, thresholdValue_,
              outputState_ ? "HIGH" : "LOW", alarmStatus_);
}

/** 모니터링 시작 메서드 */
void ThresholdLogicController::startMonitoring()
{
    const char* functionName = "startMonitoring";
    
    // 이미 스레드가 실행 중인 경우 중복 시작 방지
    if (threadRunning_) {
        asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                  "%s::%s: 모니터링 스레드가 이미 실행 중입니다\n",
                  driverName, functionName);
        return;
    }
    
    // 스레드 종료 플래그 초기화
    threadExit_ = false;
    
    // 업데이트 주기 유효성 검사 (0.1Hz ~ 1000Hz 범위)
    if (updateRate_ < 0.1 || updateRate_ > 1000.0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                  "%s::%s: 업데이트 주기가 범위를 벗어남 (%f Hz), 기본값 10Hz로 설정\n",
                  driverName, functionName, updateRate_);
        updateRate_ = 10.0;
        setDoubleParam(P_UpdateRate, updateRate_);
    }
    
    // 스레드 이름 생성
    char threadName[64];
    snprintf(threadName, sizeof(threadName), "ThresholdMonitor_%s", portName);
    
    // epicsThread 생성 및 시작
    try {
        monitorThread_ = new epicsThread(
            *this,                      // epicsThreadRunable 객체 (this)
            threadName,                 // 스레드 이름
            epicsThreadGetStackSize(epicsThreadStackMedium), // 스택 크기
            epicsThreadPriorityMedium   // 중간 우선순위
        );
        
        if (monitorThread_ == NULL) {
            ErrorHandler::handleThreadError(functionName, threadName, 
                                           "스레드 객체 생성 실패", pasynUserSelf);
            return;
        }
        
        // 스레드 시작
        monitorThread_->start();
        threadRunning_ = true;
        
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s::%s: 모니터링 스레드 시작됨 - 업데이트 주기: %f Hz\n",
                  driverName, functionName, updateRate_);
        
    } catch (std::exception& e) {
        ErrorHandler::handleThreadError(functionName, threadName, e.what(), pasynUserSelf);
        
        // 실패 시 정리
        if (monitorThread_) {
            delete monitorThread_;
            monitorThread_ = NULL;
        }
        threadRunning_ = false;
    }
}

/** 모니터링 중지 메서드 */
void ThresholdLogicController::stopMonitoring()
{
    const char* functionName = "stopMonitoring";
    
    // 스레드가 실행 중이지 않은 경우
    if (!threadRunning_) {
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s::%s: 모니터링 스레드가 실행 중이지 않습니다\n",
                  driverName, functionName);
        return;
    }
    
    // 스레드 종료 신호 설정
    threadExit_ = true;
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 모니터링 스레드 종료 신호 전송\n",
              driverName, functionName);
    
    // 스레드가 종료될 때까지 대기 (최대 5초)
    if (monitorThread_) {
        int waitCount = 0;
        const int maxWaitCount = 50; // 5초 (100ms * 50)
        
        while (threadRunning_ && waitCount < maxWaitCount) {
            epicsThreadSleep(0.1); // 100ms 대기
            waitCount++;
        }
        
        if (threadRunning_) {
            // 스레드가 정상적으로 종료되지 않은 경우 강제 종료
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                      "%s::%s: 스레드가 정상 종료되지 않아 강제 종료합니다\n",
                      driverName, functionName);
        } else {
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s::%s: 스레드가 정상적으로 종료되었습니다\n",
                      driverName, functionName);
        }
        
        // 스레드 객체 삭제
        try {
            delete monitorThread_;
            monitorThread_ = NULL;
        } catch (std::exception& e) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: 스레드 삭제 중 예외 발생: %s\n",
                      driverName, functionName, e.what());
        }
    }
    
    // 상태 변수 초기화
    threadRunning_ = false;
    threadExit_ = false;
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 모니터링 중지 완료\n",
              driverName, functionName);
}

/** epicsThreadRunable 인터페이스 구현 - 주기적 데이터 수집 및 임계값 로직 처리 */
void ThresholdLogicController::run()
{
    const char* functionName = "run";
    
    // 스레드 시작 로그
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 모니터링 스레드 시작 - PID: %lu\n",
              driverName, functionName, (unsigned long)epicsThreadGetIdSelf());
    
    // 업데이트 주기 계산 (Hz를 초 단위로 변환)
    double sleepTime = 1.0 / updateRate_;
    
    // 성능 모니터링 변수
    int cycleCount = 0;
    epicsTimeStamp startTime, currentTime;
    epicsTimeGetCurrent(&startTime);
    
    // 메인 모니터링 루프
    while (!threadExit_) {
        try {
            // 루프 시작 시간 기록
            epicsTimeStamp loopStart;
            epicsTimeGetCurrent(&loopStart);
            
            // 컨트롤러가 활성화된 경우에만 임계값 로직 처리
            if (enabled_) {
                // 임계값 로직 처리 (메인 기능)
                processThresholdLogic();
                
                // 처리 완료 후 매개변수 콜백 호출 (클라이언트 업데이트)
                callParamCallbacks();
            } else {
                // 비활성화 상태에서는 현재 값만 업데이트 (모니터링 유지)
                asynStatus status = readCurrentValueFromDevice();
                if (status == asynSuccess) {
                    setDoubleParam(P_CurrentValue, currentValue_);
                    callParamCallbacks();
                }
            }
            
            // 주기적 성능 리포트 (1000 사이클마다)
            cycleCount++;
            if (cycleCount % 1000 == 0) {
                epicsTimeGetCurrent(&currentTime);
                double elapsedTime = epicsTimeDiffInSeconds(&currentTime, &startTime);
                double actualRate = cycleCount / elapsedTime;
                
                asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                          "%s::%s: 성능 리포트 - 사이클: %d, 실제 주기: %.2f Hz, 목표 주기: %.2f Hz\n",
                          driverName, functionName, cycleCount, actualRate, updateRate_);
                
                // 카운터 및 시간 리셋
                cycleCount = 0;
                startTime = currentTime;
            }
            
            // 루프 처리 시간 계산
            epicsTimeStamp loopEnd;
            epicsTimeGetCurrent(&loopEnd);
            double processingTime = epicsTimeDiffInSeconds(&loopEnd, &loopStart);
            
            // 처리 시간이 업데이트 주기보다 긴 경우 경고
            if (processingTime > sleepTime) {
                asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                          "%s::%s: 처리 시간 초과 - 처리시간: %.3f초, 목표주기: %.3f초\n",
                          driverName, functionName, processingTime, sleepTime);
            }
            
            // 남은 시간만큼 대기 (정확한 주기 유지)
            double remainingSleepTime = sleepTime - processingTime;
            if (remainingSleepTime > 0.001) { // 최소 1ms 대기
                epicsThreadSleep(remainingSleepTime);
            } else {
                // 처리 시간이 너무 길어서 대기할 시간이 없는 경우 최소 대기
                epicsThreadSleep(0.001); // 1ms 최소 대기
            }
            
        } catch (std::exception& e) {
            // 예외 발생 시 로그 출력 및 계속 실행
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: 스레드 루프 중 예외 발생: %s\n",
                      driverName, functionName, e.what());
            
            // 예외 발생 시 잠시 대기 후 계속
            epicsThreadSleep(1.0);
        } catch (...) {
            // 알 수 없는 예외 처리
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: 알 수 없는 예외 발생\n",
                      driverName, functionName);
            
            // 예외 발생 시 잠시 대기 후 계속
            epicsThreadSleep(1.0);
        }
        
        // 업데이트 주기가 변경된 경우 새로운 주기로 업데이트
        double newSleepTime = 1.0 / updateRate_;
        if (fabs(newSleepTime - sleepTime) > 0.001) { // 1ms 이상 차이나는 경우
            sleepTime = newSleepTime;
            asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                      "%s::%s: 업데이트 주기 변경됨: %.2f Hz (%.3f초 간격)\n",
                      driverName, functionName, updateRate_, sleepTime);
        }
    }
    
    // 스레드 종료 처리
    threadRunning_ = false;
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 모니터링 스레드 종료 - 총 사이클: %d\n",
              driverName, functionName, cycleCount);
}

/** 정적 스레드 함수 - 호환성을 위해 유지 (사용되지 않음) */
void ThresholdLogicController::monitorThreadFunc(void* param)
{
    const char* functionName = "monitorThreadFunc";
    
    // 이 함수는 더 이상 사용되지 않음 (run() 메서드로 대체됨)
    printf("%s::%s: 경고 - 이 함수는 더 이상 사용되지 않습니다. run() 메서드를 사용하세요.\n", 
           driverName, functionName);
}

/** 장치에서 현재 값을 읽어오는 메서드 
 * 
 * 이 메서드는 연결된 장치 포트를 통해 아날로그 입력 값을 읽어옵니다.
 * 실제 구현에서는 asyn 클라이언트를 통해 장치와 통신합니다.
 */
asynStatus ThresholdLogicController::readCurrentValueFromDevice()
{
    const char* functionName = "readCurrentValueFromDevice";
    asynStatus status = asynSuccess;
    asynUser *pasynUser = NULL;
    
    // asyn 클라이언트 생성 (장치 포트에 연결)
    status = pasynOctetSyncIO->connect(devicePortName_, deviceAddr_, &pasynUser, NULL);
    if (status != asynSuccess) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 포트 %s에 연결 실패\n",
                  driverName, functionName, devicePortName_);
        return status;
    }
    
    try {
        // 실제 구현에서는 여기서 장치별 프로토콜에 따라 값을 읽어옴
        // 현재는 시뮬레이션을 위해 간단한 값을 생성
        // 실제 환경에서는 measComp 드라이버를 통해 USB1608G-2AO에서 값을 읽어옴
        
        // 시뮬레이션: 시간에 따라 변화하는 사인파 값 생성 (테스트용)
        epicsTimeStamp now;
        epicsTimeGetCurrent(&now);
        double timeSeconds = now.secPastEpoch + now.nsec / 1e9;
        
        // 0.0 ~ 10.0V 범위의 사인파 + 노이즈
        currentValue_ = 5.0 + 4.0 * sin(timeSeconds * 0.1) + 0.1 * (rand() / (double)RAND_MAX - 0.5);
        
        // 값의 유효성 검사
        if (currentValue_ < -10.0 || currentValue_ > 10.0) {
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                      "%s::%s: 읽어온 값이 범위를 벗어남: %f\n",
                      driverName, functionName, currentValue_);
            currentValue_ = fmax(-10.0, fmin(10.0, currentValue_)); // 범위 제한
        }
        
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 장치에서 값 읽기 성공: %f V\n",
                  driverName, functionName, currentValue_);
        
    } catch (...) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 읽기 중 예외 발생\n",
                  driverName, functionName);
        status = asynError;
    }
    
    // asyn 클라이언트 연결 해제
    if (pasynUser) {
        pasynOctetSyncIO->disconnect(pasynUser);
    }
    
    return status;
}

/** 장치에 출력 상태를 설정하는 메서드 
 * 
 * 이 메서드는 연결된 장치 포트를 통해 디지털 출력 상태를 설정합니다.
 * 실제 구현에서는 asyn 클라이언트를 통해 장치와 통신합니다.
 */
asynStatus ThresholdLogicController::writeOutputStateToDevice(bool state)
{
    const char* functionName = "writeOutputStateToDevice";
    asynStatus status = asynSuccess;
    asynUser *pasynUser = NULL;
    
    // asyn 클라이언트 생성 (장치 포트에 연결)
    status = pasynOctetSyncIO->connect(devicePortName_, deviceAddr_, &pasynUser, NULL);
    if (status != asynSuccess) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 포트 %s에 연결 실패\n",
                  driverName, functionName, devicePortName_);
        return status;
    }
    
    try {
        // 실제 구현에서는 여기서 장치별 프로토콜에 따라 출력을 설정
        // 현재는 시뮬레이션을 위해 로그만 출력
        // 실제 환경에서는 measComp 드라이버를 통해 USB1608G-2AO의 디지털 출력을 제어
        
        // 시뮬레이션: 출력 상태 설정 명령 전송 (테스트용)
        const char* stateStr = state ? "HIGH" : "LOW";
        int digitalValue = state ? 1 : 0;
        
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DEVICE,
                  "%s::%s: 장치에 디지털 출력 설정 - 상태: %s (값: %d)\n",
                  driverName, functionName, stateStr, digitalValue);
        
        // 실제 구현에서는 여기서 다음과 같은 작업을 수행:
        // 1. 장치의 디지털 출력 레지스터에 값 쓰기
        // 2. 명령 전송 및 응답 확인
        // 3. 오류 상태 검사
        
        // 시뮬레이션: 성공적으로 설정되었다고 가정
        asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                  "%s::%s: 출력 상태 설정 완료: %s\n",
                  driverName, functionName, stateStr);
        
    } catch (...) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 쓰기 중 예외 발생\n",
                  driverName, functionName);
        status = asynError;
    }
    
    // asyn 클라이언트 연결 해제
    if (pasynUser) {
        pasynOctetSyncIO->disconnect(pasynUser);
    }
    
    return status;
}

/** 알람 상태 업데이트 메서드 
 * 
 * 이 메서드는 현재 알람 상태에 따라 EPICS 알람 시스템을 업데이트합니다.
 * 알람 심각도와 상태를 설정하여 클라이언트에 알림을 제공합니다.
 */
void ThresholdLogicController::updateAlarmStatus()
{
    const char* functionName = "updateAlarmStatus";
    int alarmSeverity = 0;  // NO_ALARM
    int alarmStatus = 0;    // NO_ALARM
    
    // 알람 상태에 따른 심각도 및 상태 설정
    switch (alarmStatus_) {
        case 0:  // 정상 상태
            alarmSeverity = 0;  // NO_ALARM
            alarmStatus = 0;    // NO_ALARM
            break;
            
        case 1:  // 경고 상태 (MINOR)
            alarmSeverity = 1;  // MINOR_ALARM
            alarmStatus = 3;    // STATE_ALARM
            asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                      "%s::%s: MINOR 알람 설정 - 경고 상태\n",
                      driverName, functionName);
            break;
            
        case 2:  // 주요 오류 상태 (MAJOR)
            alarmSeverity = 2;  // MAJOR_ALARM
            alarmStatus = 4;    // COMM_ALARM (통신 오류)
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: MAJOR 알람 설정 - 통신 오류\n",
                      driverName, functionName);
            break;
            
        case 3:  // 치명적 오류 상태 (INVALID)
            alarmSeverity = 3;  // INVALID_ALARM
            alarmStatus = 17;   // UDF_ALARM (정의되지 않은 값)
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: INVALID 알람 설정 - 치명적 오류\n",
                      driverName, functionName);
            break;
            
        default:
            // 알 수 없는 알람 상태
            alarmSeverity = 2;  // MAJOR_ALARM
            alarmStatus = 17;   // UDF_ALARM
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                      "%s::%s: 알 수 없는 알람 상태: %d\n",
                      driverName, functionName, alarmStatus_);
            break;
    }
    
    // 주요 매개변수들에 알람 상태 설정
    // 현재 값 매개변수에 알람 설정
    setParamAlarmStatus(P_CurrentValue, alarmStatus);
    setParamAlarmSeverity(P_CurrentValue, alarmSeverity);
    
    // 출력 상태 매개변수에 알람 설정
    setParamAlarmStatus(P_OutputState, alarmStatus);
    setParamAlarmSeverity(P_OutputState, alarmSeverity);
    
    // 알람 상태 매개변수 자체 업데이트
    setIntegerParam(P_AlarmStatus, alarmStatus_);
    setParamAlarmStatus(P_AlarmStatus, alarmStatus);
    setParamAlarmSeverity(P_AlarmStatus, alarmSeverity);
    
    // 디버그 정보 출력
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 알람 상태 업데이트 완료 - 상태: %d, 심각도: %d, EPICS상태: %d\n",
              driverName, functionName, alarmStatus_, alarmSeverity, alarmStatus);
}

/** 매개변수 유효성 검사 메서드 */
bool ThresholdLogicController::validateParameters()
{
    const char* functionName = "validateParameters";
    bool isValid = true;
    
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s::%s: 매개변수 유효성 검사 시작\n",
              driverName, functionName);
    
    // 1. 임계값 유효성 검사
    if (thresholdValue_ < -10.0 || thresholdValue_ > 10.0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 임계값이 유효 범위를 벗어남: %f (범위: -10.0 ~ +10.0 V)\n",
                  driverName, functionName, thresholdValue_);
        isValid = false;
    }
    
    // 2. 히스테리시스 유효성 검사
    if (hysteresis_ < 0.0 || hysteresis_ > 5.0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 히스테리시스가 유효 범위를 벗어남: %f (범위: 0.0 ~ 5.0 V)\n",
                  driverName, functionName, hysteresis_);
        isValid = false;
    }
    
    // 3. 임계값과 히스테리시스 관계 검사
    if (hysteresis_ > fabs(thresholdValue_)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                  "%s::%s: 히스테리시스가 임계값의 절댓값보다 큼 - 히스테리시스: %f, 임계값: %f\n",
                  driverName, functionName, hysteresis_, thresholdValue_);
        // 경고이지만 동작은 가능하므로 isValid는 false로 설정하지 않음
    }
    
    // 4. 업데이트 주기 유효성 검사
    if (updateRate_ < 0.1 || updateRate_ > 1000.0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 업데이트 주기가 유효 범위를 벗어남: %f (범위: 0.1 ~ 1000.0 Hz)\n",
                  driverName, functionName, updateRate_);
        isValid = false;
    }
    
    // 5. 장치 포트 이름 검사
    if (strlen(devicePortName_) == 0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 포트 이름이 설정되지 않음\n",
                  driverName, functionName);
        isValid = false;
    } else if (strlen(devicePortName_) >= sizeof(devicePortName_)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 포트 이름이 너무 김: %s\n",
                  driverName, functionName, devicePortName_);
        isValid = false;
    }
    
    // 6. 장치 주소 유효성 검사
    if (deviceAddr_ < 0 || deviceAddr_ > 255) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 장치 주소가 유효 범위를 벗어남: %d (범위: 0-255)\n",
                  driverName, functionName, deviceAddr_);
        isValid = false;
    }
    
    // 7. 현재값 유효성 검사 (경고만 출력)
    if (currentValue_ < -10.0 || currentValue_ > 10.0) {
        asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                  "%s::%s: 현재값이 예상 범위를 벗어남: %f V (예상 범위: -10.0 ~ +10.0 V)\n",
                  driverName, functionName, currentValue_);
        // 현재값은 측정값이므로 유효성 검사 실패로 처리하지 않음
    }
    
    // 8. 알람 상태 유효성 검사
    if (alarmStatus_ < 0 || alarmStatus_ > 3) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 알람 상태가 유효 범위를 벗어남: %d (범위: 0-3)\n",
                  driverName, functionName, alarmStatus_);
        // 알람 상태를 정상으로 리셋
        alarmStatus_ = 0;
        setIntegerParam(P_AlarmStatus, alarmStatus_);
    }
    
    // 9. 논리적 일관성 검사
    if (enabled_ && !threadRunning_) {
        asynPrint(pasynUserSelf, ASYN_TRACE_WARNING,
                  "%s::%s: 활성화 상태이지만 모니터링 스레드가 실행되지 않음\n",
                  driverName, functionName);
        // 이는 일시적인 상태일 수 있으므로 오류로 처리하지 않음
    }
    
    // 10. 메모리 및 리소스 상태 검사
    if (enabled_ && monitorThread_ == NULL) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 활성화 상태이지만 모니터링 스레드 객체가 NULL임\n",
                  driverName, functionName);
        isValid = false;
    }
    
    // 검사 결과 로깅
    if (isValid) {
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s::%s: 모든 매개변수가 유효함\n",
                  driverName, functionName);
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s::%s: 현재 설정 - 임계값: %f V, 히스테리시스: %f V, 주기: %f Hz\n",
                  driverName, functionName, thresholdValue_, hysteresis_, updateRate_);
    } else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                  "%s::%s: 매개변수 유효성 검사 실패 - 설정을 확인하세요\n",
                  driverName, functionName);
    }
    
    return isValid;
}



/* IOC 쉘 명령어 구현 */

/** ThresholdLogicConfig IOC 쉘 명령어 구현
 * 
 * 새로운 ThresholdLogicController 인스턴스를 생성하고 구성합니다.
 * 
 * 매개변수:
 *   portName   - 생성할 asyn 포트의 이름 (문자열)
 *   devicePort - 연결할 장치 포트의 이름 (문자열)  
 *   deviceAddr - 장치 주소 (정수, 0-255 범위)
 * 
 * 반환값:
 *   0  - 성공
 *   -1 - 실패
 * 
 * 사용 예:
 *   ThresholdLogicConfig("THRESHOLD1", "USB1608G_2AO_cpp_PORT", 0)
 */
extern "C" int ThresholdLogicConfig(const char* portName, const char* devicePort, int deviceAddr)
{
    const char* functionName = "ThresholdLogicConfig";
    
    // 입력 매개변수 유효성 검사
    if (portName == NULL || strlen(portName) == 0) {
        printf("%s 오류: 포트 이름이 NULL이거나 비어있습니다\n", functionName);
        return -1;
    }
    
    if (devicePort == NULL || strlen(devicePort) == 0) {
        printf("%s 오류: 장치 포트 이름이 NULL이거나 비어있습니다\n", functionName);
        return -1;
    }
    
    if (deviceAddr < 0 || deviceAddr > 255) {
        printf("%s 오류: 장치 주소가 유효 범위(0-255)를 벗어났습니다: %d\n", 
               functionName, deviceAddr);
        return -1;
    }
    
    // 포트 이름 중복 검사
    if (findAsynPortDriver(portName) != NULL) {
        printf("%s 오류: 포트 이름 '%s'이 이미 사용 중입니다\n", 
               functionName, portName);
        return -1;
    }
    
    try {
        // ThresholdLogicController 인스턴스 생성
        ThresholdLogicController* pController = new ThresholdLogicController(portName, devicePort, deviceAddr);
        
        if (pController == NULL) {
            printf("%s 오류: ThresholdLogicController 생성 실패\n", functionName);
            return -1;
        }
        
        printf("%s: 성공적으로 생성됨 - 포트: %s, 장치포트: %s, 주소: %d\n",
               functionName, portName, devicePort, deviceAddr);
        
        return 0;
        
    } catch (std::bad_alloc& e) {
        printf("%s 오류: 메모리 할당 실패 - %s\n", functionName, e.what());
        return -1;
    } catch (std::exception& e) {
        printf("%s 오류: 예외 발생 - %s\n", functionName, e.what());
        return -1;
    } catch (...) {
        printf("%s 오류: 알 수 없는 예외 발생\n", functionName);
        return -1;
    }
}

/** ThresholdLogicHelp IOC 쉘 명령어 구현 - 사용법 도움말 표시 */
extern "C" void ThresholdLogicHelp(void)
{
    printf("\n=== ThresholdLogicController 사용 가이드 ===\n\n");
    
    printf("1. ThresholdLogicConfig - 임계값 로직 컨트롤러 생성\n");
    printf("   사용법: ThresholdLogicConfig(portName, devicePort, deviceAddr)\n");
    printf("   매개변수:\n");
    printf("     portName   : 생성할 asyn 포트 이름 (문자열)\n");
    printf("     devicePort : 연결할 장치 포트 이름 (문자열)\n");
    printf("     deviceAddr : 장치 주소 (정수, 0-255)\n");
    printf("   예제:\n");
    printf("     ThresholdLogicConfig(\"THRESHOLD1\", \"USB1608G_2AO_cpp_PORT\", 0)\n\n");
    
    printf("2. 주요 기능:\n");
    printf("   - 아날로그 입력 값 실시간 모니터링\n");
    printf("   - 설정 가능한 임계값과 히스테리시스\n");
    printf("   - 디지털 출력 자동 제어\n");
    printf("   - EPICS 레코드를 통한 원격 제어\n");
    printf("   - 알람 및 상태 모니터링\n\n");
    
    printf("3. 데이터베이스 레코드 접근:\n");
    printf("   $(P)$(R)Threshold     - 임계값 설정 (V)\n");
    printf("   $(P)$(R)CurrentValue  - 현재 측정값 (V)\n");
    printf("   $(P)$(R)OutputState   - 출력 상태 (0/1)\n");
    printf("   $(P)$(R)Enable        - 활성화 제어 (0/1)\n");
    printf("   $(P)$(R)Hysteresis    - 히스테리시스 값 (V)\n");
    printf("   $(P)$(R)UpdateRate    - 업데이트 주기 (Hz)\n");
    printf("   $(P)$(R)AlarmStatus   - 알람 상태\n\n");
    
    printf("4. 일반적인 사용 순서:\n");
    printf("   a) ThresholdLogicConfig로 컨트롤러 생성\n");
    printf("   b) 데이터베이스 템플릿 로드\n");
    printf("   c) 임계값 및 히스테리시스 설정\n");
    printf("   d) Enable 레코드로 모니터링 시작\n\n");
    
    printf("5. 문제 해결:\n");
    printf("   - 포트 이름 중복: 다른 포트 이름 사용\n");
    printf("   - 장치 연결 실패: 장치 포트 및 주소 확인\n");
    printf("   - 알람 발생: AlarmStatus 레코드 확인\n");
    printf("   - 성능 문제: UpdateRate 조정\n\n");
    
    printf("자세한 정보는 ThresholdLogicController 문서를 참조하세요.\n");
    printf("===============================================\n\n");
}

/* IOC 쉘 명령어 등록 구조체 정의 */

// ThresholdLogicConfig 명령어 인수 정의
static const iocshArg thresholdConfigArg0 = {
    "portName", 
    iocshArgString
};
static const iocshArg thresholdConfigArg1 = {
    "devicePort", 
    iocshArgString
};
static const iocshArg thresholdConfigArg2 = {
    "deviceAddr", 
    iocshArgInt
};

static const iocshArg *thresholdConfigArgs[] = {
    &thresholdConfigArg0,
    &thresholdConfigArg1,
    &thresholdConfigArg2
};

// ThresholdLogicConfig 명령어 정의
static const iocshFuncDef thresholdConfigFuncDef = {
    "ThresholdLogicConfig",                    // 명령어 이름
    3,                                         // 인수 개수
    thresholdConfigArgs                        // 인수 배열
};

// ThresholdLogicHelp 명령어 정의 (인수 없음)
static const iocshFuncDef thresholdHelpFuncDef = {
    "ThresholdLogicHelp",                      // 명령어 이름
    0,                                         // 인수 개수
    NULL                                       // 인수 없음
};

/* IOC 쉘 명령어 콜백 함수들 */

/** ThresholdLogicConfig 명령어 콜백 함수 */
static void thresholdConfigCallFunc(const iocshArgBuf *args)
{
    // 인수 유효성 검사
    if (args == NULL) {
        printf("ThresholdLogicConfig: 인수가 NULL입니다\n");
        return;
    }
    
    // 명령어 실행 및 결과 처리
    int result = ThresholdLogicConfig(args[0].sval, args[1].sval, args[2].ival);
    
    if (result != 0) {
        printf("ThresholdLogicConfig: 명령어 실행 실패 (반환값: %d)\n", result);
        printf("도움말을 보려면 'ThresholdLogicHelp'를 입력하세요.\n");
    }
}

/** ThresholdLogicHelp 명령어 콜백 함수 */
static void thresholdHelpCallFunc(const iocshArgBuf *args)
{
    // 인수는 사용하지 않음 (도움말 명령어)
    (void)args; // 컴파일러 경고 방지
    
    ThresholdLogicHelp();
}

/** IOC 쉘 명령어 등록 함수 
 * 
 * 이 함수는 EPICS IOC 시작 시 자동으로 호출되어
 * ThresholdLogicController 관련 명령어들을 IOC 쉘에 등록합니다.
 */
extern "C" void ThresholdLogicRegister(void)
{
    const char* functionName = "ThresholdLogicRegister";
    
    // ThresholdLogicConfig 명령어 등록
    iocshRegister(&thresholdConfigFuncDef, thresholdConfigCallFunc);
    
    // ThresholdLogicHelp 명령어 등록
    iocshRegister(&thresholdHelpFuncDef, thresholdHelpCallFunc);
    
    printf("%s: IOC 쉘 명령어 등록 완료\n", functionName);
    printf("  - ThresholdLogicConfig: 임계값 로직 컨트롤러 생성\n");
    printf("  - ThresholdLogicHelp: 사용법 도움말 표시\n");
    printf("도움말을 보려면 'ThresholdLogicHelp'를 입력하세요.\n");
}

/** 구성 유효성 검사 (ErrorHandler 사용) */
bool ThresholdLogicController::validateConfigurationWithErrorHandler()
{
    const char* functionName = "validateConfigurationWithErrorHandler";
    
    // ErrorHandler::ThresholdConfig 구조체 생성
    ErrorHandler::ThresholdConfig config;
    strncpy(config.portName, portName, sizeof(config.portName) - 1);
    config.portName[sizeof(config.portName) - 1] = '\0';
    
    strncpy(config.devicePort, devicePortName_, sizeof(config.devicePort) - 1);
    config.devicePort[sizeof(config.devicePort) - 1] = '\0';
    
    config.deviceAddr = deviceAddr_;
    config.updateRate = updateRate_;
    config.priority = 50; // 기본 우선순위
    config.thresholdValue = thresholdValue_;
    config.hysteresis = hysteresis_;
    
    // ErrorHandler를 사용한 유효성 검사
    ErrorHandler::ValidationResult result = ErrorHandler::validateConfiguration(config);
    
    if (!result.isValid) {
        ErrorHandler::logError(result.errorLevel, functionName, result.errorMessage, pasynUserSelf);
        if (strlen(result.suggestion) > 0) {
            ErrorHandler::logError(ErrorHandler::INFO, functionName, 
                                  result.suggestion, pasynUserSelf);
        }
        return false;
    }
    
    if (result.errorLevel == ErrorHandler::WARNING) {
        ErrorHandler::logError(ErrorHandler::WARNING, functionName, 
                              result.errorMessage, pasynUserSelf);
        if (strlen(result.suggestion) > 0) {
            ErrorHandler::logError(ErrorHandler::INFO, functionName, 
                                  result.suggestion, pasynUserSelf);
        }
    }
    
    return true;
}

/** 오류 로깅 (ErrorHandler 사용) */
void ThresholdLogicController::logError(const char* functionName, const char* message)
{
    if (functionName == NULL || message == NULL) {
        ErrorHandler::logError(ErrorHandler::ERROR, "ThresholdLogicController::logError", 
                              "NULL 포인터가 전달됨");
        return;
    }
    
    // 전체 함수 이름 구성
    char fullFunctionName[128];
    snprintf(fullFunctionName, sizeof(fullFunctionName), 
            "ThresholdLogicController::%s", functionName);
    
    // ErrorHandler를 통한 로깅
    ErrorHandler::logError(ErrorHandler::ERROR, fullFunctionName, message, pasynUserSelf);
}
/* EPICS 등록자 내보내기 */
epicsExportRegistrar(ThresholdLogicRegister);
