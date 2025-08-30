/* ErrorHandler.cpp
 *
 * 오류 처리 및 로깅을 위한 유틸리티 클래스 구현
 * 
 * 이 클래스는 EPICS IOC 애플리케이션에서 발생하는 다양한 오류를 
 * 분류하고 로깅하며, EPICS 알람 시스템과 통합하여 상태를 보고합니다.
 *
 * Author: EPICS IOC Development Guide
 * Date: 2025
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#include <epicsTime.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsMutex.h>
#include <asynDriver.h>

#include "ErrorHandler.h"

// 정적 멤버 변수 초기화
int ErrorHandler::infoCount_ = 0;
int ErrorHandler::warningCount_ = 0;
int ErrorHandler::errorCount_ = 0;
int ErrorHandler::fatalCount_ = 0;

// 스레드 안전성을 위한 뮤텍스
static epicsMutexId errorStatsMutex = NULL;

// 뮤텍스 초기화 (한 번만 실행)
static void initializeMutex() {
    if (errorStatsMutex == NULL) {
        errorStatsMutex = epicsMutexCreate();
    }
}

/** 오류 로깅 메서드 */
void ErrorHandler::logError(ErrorLevel level, const char* source, const char* message, 
                           asynUser* pasynUser)
{
    if (source == NULL || message == NULL) {
        printf("ErrorHandler::logError: NULL 포인터가 전달됨\n");
        return;
    }
    
    // 뮤텍스 초기화 확인
    initializeMutex();
    
    // 통계 업데이트 (스레드 안전)
    epicsMutexLock(errorStatsMutex);
    switch (level) {
        case INFO:    infoCount_++;    break;
        case WARNING: warningCount_++; break;
        case ERROR:   errorCount_++;   break;
        case FATAL:   fatalCount_++;   break;
    }
    epicsMutexUnlock(errorStatsMutex);
    
    // 내부 로깅 호출
    internalLog(level, source, message, pasynUser);
}

/** 상세 오류 로깅 메서드 */
void ErrorHandler::logDetailedError(ErrorLevel level, const char* source, const char* message,
                                   const char* details, int errorCode, asynUser* pasynUser)
{
    if (source == NULL || message == NULL) {
        printf("ErrorHandler::logDetailedError: NULL 포인터가 전달됨\n");
        return;
    }
    
    // 상세 메시지 구성
    char detailedMessage[512];
    if (details != NULL && strlen(details) > 0) {
        snprintf(detailedMessage, sizeof(detailedMessage), 
                "%s [상세: %s] [오류코드: %d]", message, details, errorCode);
    } else {
        snprintf(detailedMessage, sizeof(detailedMessage), 
                "%s [오류코드: %d]", message, errorCode);
    }
    
    // 일반 로깅 메서드 호출
    logError(level, source, detailedMessage, pasynUser);
}

/** EPICS 알람 상태 설정 */
asynStatus ErrorHandler::setAlarmStatus(asynUser* pasynUser, AlarmStatus status, AlarmSeverity severity)
{
    if (pasynUser == NULL) {
        logError(ERROR, "ErrorHandler::setAlarmStatus", "NULL asynUser 포인터");
        return asynError;
    }
    
    // EPICS 알람 설정 (실제 구현에서는 asynPortDriver의 setParamAlarmStatus 사용)
    // 여기서는 로깅만 수행
    char alarmMessage[256];
    snprintf(alarmMessage, sizeof(alarmMessage),
            "알람 설정 - 상태: %s, 심각도: %s",
            alarmStatusToString(status), alarmSeverityToString(severity));
    
    ErrorLevel logLevel = INFO;
    switch (severity) {
        case NO_ALARM:      logLevel = INFO;    break;
        case MINOR_ALARM:   logLevel = WARNING; break;
        case MAJOR_ALARM:   logLevel = ERROR;   break;
        case INVALID_ALARM: logLevel = FATAL;   break;
    }
    
    logError(logLevel, "ErrorHandler::setAlarmStatus", alarmMessage, pasynUser);
    
    return asynSuccess;
}

/** ThresholdLogicController 구성 유효성 검사 */
ErrorHandler::ValidationResult ErrorHandler::validateConfiguration(const ThresholdConfig& config)
{
    ValidationResult result;
    result.isValid = true;
    result.errorLevel = INFO;
    strcpy(result.errorMessage, "");
    strcpy(result.suggestion, "");
    
    // 포트 이름 검사
    if (!validateStringParameter("portName", config.portName, sizeof(config.portName), 
                                false, "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "포트 이름이 유효하지 않습니다");
        strcpy(result.suggestion, "1-63자의 영숫자와 언더스코어만 사용하세요");
        return result;
    }
    
    // 장치 포트 이름 검사
    if (!validateStringParameter("devicePort", config.devicePort, sizeof(config.devicePort), 
                                false, "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "장치 포트 이름이 유효하지 않습니다");
        strcpy(result.suggestion, "유효한 asyn 포트 이름을 지정하세요");
        return result;
    }
    
    // 장치 주소 검사
    if (!validateIntParameter("deviceAddr", config.deviceAddr, 0, 255, 
                             "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "장치 주소가 유효 범위를 벗어났습니다");
        strcpy(result.suggestion, "0-255 범위의 값을 사용하세요");
        return result;
    }
    
    // 업데이트 주기 검사
    if (!validateParameter("updateRate", config.updateRate, 0.1, 1000.0, 
                          "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "업데이트 주기가 유효 범위를 벗어났습니다");
        strcpy(result.suggestion, "0.1-1000.0 Hz 범위의 값을 사용하세요");
        return result;
    }
    
    // 스레드 우선순위 검사
    if (!validateIntParameter("priority", config.priority, 0, 99, 
                             "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = WARNING;
        strcpy(result.errorMessage, "스레드 우선순위가 권장 범위를 벗어났습니다");
        strcpy(result.suggestion, "0-99 범위의 값을 사용하세요 (기본값: 50)");
        // 경고이므로 계속 검사
    }
    
    // 임계값 검사
    if (!validateParameter("thresholdValue", config.thresholdValue, -10.0, 10.0, 
                          "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "임계값이 유효 범위를 벗어났습니다");
        strcpy(result.suggestion, "-10.0V ~ +10.0V 범위의 값을 사용하세요");
        return result;
    }
    
    // 히스테리시스 검사
    if (!validateParameter("hysteresis", config.hysteresis, 0.0, 5.0, 
                          "ErrorHandler::validateConfiguration")) {
        result.isValid = false;
        result.errorLevel = ERROR;
        strcpy(result.errorMessage, "히스테리시스가 유효 범위를 벗어났습니다");
        strcpy(result.suggestion, "0.0V ~ 5.0V 범위의 값을 사용하세요");
        return result;
    }
    
    // 임계값과 히스테리시스 관계 검사
    if (config.hysteresis > fabs(config.thresholdValue)) {
        result.isValid = false;
        result.errorLevel = WARNING;
        strcpy(result.errorMessage, "히스테리시스가 임계값보다 큽니다");
        strcpy(result.suggestion, "히스테리시스를 임계값 절댓값보다 작게 설정하세요");
        // 경고이므로 유효성은 통과로 처리
        result.isValid = true;
    }
    
    // 모든 검사 통과
    if (result.isValid && result.errorLevel == INFO) {
        strcpy(result.errorMessage, "구성이 유효합니다");
        strcpy(result.suggestion, "");
    }
    
    return result;
}

/** 런타임 오류 처리 */
bool ErrorHandler::handleRuntimeError(const char* source, const char* errorType, 
                                     int errorCode, asynUser* pasynUser)
{
    if (source == NULL || errorType == NULL) {
        logError(ERROR, "ErrorHandler::handleRuntimeError", "NULL 포인터가 전달됨");
        return false;
    }
    
    char errorMessage[256];
    snprintf(errorMessage, sizeof(errorMessage), 
            "런타임 오류 발생 - 유형: %s, 코드: %d", errorType, errorCode);
    
    ErrorLevel level = ERROR;
    bool recoverable = true;
    
    // 오류 유형별 처리
    if (strcmp(errorType, "MEMORY_ALLOCATION") == 0) {
        level = FATAL;
        recoverable = false;
        logError(level, source, "메모리 할당 실패 - 시스템 재시작 필요", pasynUser);
    }
    else if (strcmp(errorType, "THREAD_CREATION") == 0) {
        level = ERROR;
        recoverable = true;
        logError(level, source, "스레드 생성 실패 - 재시도 가능", pasynUser);
    }
    else if (strcmp(errorType, "PARAMETER_VALIDATION") == 0) {
        level = WARNING;
        recoverable = true;
        logError(level, source, "매개변수 유효성 검사 실패 - 기본값 사용", pasynUser);
    }
    else if (strcmp(errorType, "DEVICE_COMMUNICATION") == 0) {
        level = ERROR;
        recoverable = true;
        logError(level, source, "장치 통신 오류 - 연결 확인 필요", pasynUser);
    }
    else if (strcmp(errorType, "TIMEOUT") == 0) {
        level = WARNING;
        recoverable = true;
        logError(level, source, "타임아웃 발생 - 재시도 권장", pasynUser);
    }
    else {
        // 알 수 없는 오류 유형
        logError(level, source, errorMessage, pasynUser);
    }
    
    return recoverable;
}

/** 통신 오류 처리 */
bool ErrorHandler::handleCommunicationError(const char* source, const char* devicePort,
                                           int deviceAddr, const char* operation,
                                           asynUser* pasynUser)
{
    if (source == NULL || devicePort == NULL || operation == NULL) {
        logError(ERROR, "ErrorHandler::handleCommunicationError", "NULL 포인터가 전달됨");
        return false;
    }
    
    char errorMessage[256];
    snprintf(errorMessage, sizeof(errorMessage),
            "통신 오류 - 포트: %s, 주소: %d, 작업: %s", 
            devicePort, deviceAddr, operation);
    
    logError(ERROR, source, errorMessage, pasynUser);
    
    // 알람 설정
    if (pasynUser != NULL) {
        setAlarmStatus(pasynUser, COMM_ALARM, MAJOR_ALARM);
    }
    
    // 통신 오류는 일반적으로 재시도 가능
    return true;
}

/** 스레드 오류 처리 */
bool ErrorHandler::handleThreadError(const char* source, const char* threadName,
                                    const char* errorMessage, asynUser* pasynUser)
{
    if (source == NULL || threadName == NULL || errorMessage == NULL) {
        logError(ERROR, "ErrorHandler::handleThreadError", "NULL 포인터가 전달됨");
        return false;
    }
    
    char fullMessage[256];
    snprintf(fullMessage, sizeof(fullMessage),
            "스레드 오류 - 이름: %s, 메시지: %s", threadName, errorMessage);
    
    logError(ERROR, source, fullMessage, pasynUser);
    
    // 스레드 오류는 대부분 재시작 가능
    bool restartRecommended = true;
    
    // 특정 오류 유형에 따른 처리
    if (strstr(errorMessage, "FATAL") != NULL || strstr(errorMessage, "SEGFAULT") != NULL) {
        restartRecommended = false;
        logError(FATAL, source, "치명적 스레드 오류 - 재시작 불가", pasynUser);
    }
    
    return restartRecommended;
}

/** 매개변수 유효성 검사 */
bool ErrorHandler::validateParameter(const char* paramName, double value, 
                                   double minValue, double maxValue, const char* source)
{
    if (paramName == NULL || source == NULL) {
        logError(ERROR, "ErrorHandler::validateParameter", "NULL 포인터가 전달됨");
        return false;
    }
    
    if (isnan(value) || isinf(value)) {
        char message[256];
        snprintf(message, sizeof(message), 
                "매개변수 '%s'가 유효하지 않은 값입니다 (NaN 또는 Inf)", paramName);
        logError(ERROR, source, message);
        return false;
    }
    
    if (value < minValue || value > maxValue) {
        char message[256];
        snprintf(message, sizeof(message),
                "매개변수 '%s' 값 %f이 유효 범위 [%f, %f]를 벗어났습니다",
                paramName, value, minValue, maxValue);
        logError(WARNING, source, message);
        return false;
    }
    
    return true;
}

/** 정수 매개변수 유효성 검사 */
bool ErrorHandler::validateIntParameter(const char* paramName, int value, 
                                       int minValue, int maxValue, const char* source)
{
    if (paramName == NULL || source == NULL) {
        logError(ERROR, "ErrorHandler::validateIntParameter", "NULL 포인터가 전달됨");
        return false;
    }
    
    if (value < minValue || value > maxValue) {
        char message[256];
        snprintf(message, sizeof(message),
                "정수 매개변수 '%s' 값 %d이 유효 범위 [%d, %d]를 벗어났습니다",
                paramName, value, minValue, maxValue);
        logError(WARNING, source, message);
        return false;
    }
    
    return true;
}

/** 문자열 매개변수 유효성 검사 */
bool ErrorHandler::validateStringParameter(const char* paramName, const char* value,
                                          size_t maxLength, bool allowEmpty, const char* source)
{
    if (paramName == NULL || source == NULL) {
        logError(ERROR, "ErrorHandler::validateStringParameter", "NULL 포인터가 전달됨");
        return false;
    }
    
    if (value == NULL) {
        char message[256];
        snprintf(message, sizeof(message), "문자열 매개변수 '%s'가 NULL입니다", paramName);
        logError(ERROR, source, message);
        return false;
    }
    
    size_t length = strlen(value);
    
    if (!allowEmpty && length == 0) {
        char message[256];
        snprintf(message, sizeof(message), "문자열 매개변수 '%s'가 비어있습니다", paramName);
        logError(WARNING, source, message);
        return false;
    }
    
    if (length >= maxLength) {
        char message[256];
        snprintf(message, sizeof(message),
                "문자열 매개변수 '%s' 길이 %zu가 최대 길이 %zu를 초과했습니다",
                paramName, length, maxLength - 1);
        logError(WARNING, source, message);
        return false;
    }
    
    return true;
}

/** 오류 통계 정보 가져오기 */
void ErrorHandler::getErrorStatistics(int* infoCount, int* warningCount, 
                                     int* errorCount, int* fatalCount)
{
    initializeMutex();
    
    epicsMutexLock(errorStatsMutex);
    if (infoCount)    *infoCount = infoCount_;
    if (warningCount) *warningCount = warningCount_;
    if (errorCount)   *errorCount = errorCount_;
    if (fatalCount)   *fatalCount = fatalCount_;
    epicsMutexUnlock(errorStatsMutex);
}

/** 오류 통계 초기화 */
void ErrorHandler::resetErrorStatistics()
{
    initializeMutex();
    
    epicsMutexLock(errorStatsMutex);
    infoCount_ = 0;
    warningCount_ = 0;
    errorCount_ = 0;
    fatalCount_ = 0;
    epicsMutexUnlock(errorStatsMutex);
    
    logError(INFO, "ErrorHandler::resetErrorStatistics", "오류 통계가 초기화되었습니다");
}

/** 오류 레벨을 문자열로 변환 */
const char* ErrorHandler::errorLevelToString(ErrorLevel level)
{
    switch (level) {
        case INFO:    return "정보";
        case WARNING: return "경고";
        case ERROR:   return "오류";
        case FATAL:   return "치명적";
        default:      return "알수없음";
    }
}

/** 알람 심각도를 문자열로 변환 */
const char* ErrorHandler::alarmSeverityToString(AlarmSeverity severity)
{
    switch (severity) {
        case NO_ALARM:      return "알람없음";
        case MINOR_ALARM:   return "경미한알람";
        case MAJOR_ALARM:   return "주요알람";
        case INVALID_ALARM: return "유효하지않음";
        default:            return "알수없음";
    }
}

/** 알람 상태를 문자열로 변환 */
const char* ErrorHandler::alarmStatusToString(AlarmStatus status)
{
    switch (status) {
        case NO_ALARM_STATUS:     return "정상";
        case READ_ALARM:          return "읽기오류";
        case WRITE_ALARM:         return "쓰기오류";
        case HIHI_ALARM:          return "상한상한";
        case HIGH_ALARM:          return "상한";
        case LOLO_ALARM:          return "하한하한";
        case LOW_ALARM:           return "하한";
        case STATE_ALARM:         return "상태오류";
        case COS_ALARM:           return "변화알람";
        case COMM_ALARM:          return "통신오류";
        case TIMEOUT_ALARM:       return "타임아웃";
        case HW_LIMIT_ALARM:      return "하드웨어제한";
        case CALC_ALARM:          return "계산오류";
        case SCAN_ALARM:          return "스캔오류";
        case LINK_ALARM:          return "링크오류";
        case SOFT_ALARM:          return "소프트웨어알람";
        case BAD_SUB_ALARM:       return "잘못된서브레코드";
        case UDF_ALARM:           return "정의되지않은값";
        case DISABLE_ALARM:       return "비활성화";
        case SIMM_ALARM:          return "시뮬레이션";
        case READ_ACCESS_ALARM:   return "읽기접근오류";
        case WRITE_ACCESS_ALARM:  return "쓰기접근오류";
        default:                  return "알수없음";
    }
}

/** 내부 로깅 메서드 */
void ErrorHandler::internalLog(ErrorLevel level, const char* source, const char* message,
                              asynUser* pasynUser)
{
    // 타임스탬프 생성
    char timestamp[64];
    getTimestampString(timestamp, sizeof(timestamp));
    
    // 로그 메시지 구성
    char logMessage[512];
    snprintf(logMessage, sizeof(logMessage),
            "[%s] [%s] %s: %s",
            timestamp, errorLevelToString(level), source, message);
    
    // 콘솔 출력
    printf("%s\n", logMessage);
    
    // asyn 트레이스 출력 (pasynUser가 있는 경우)
    if (pasynUser != NULL) {
        int traceLevel = convertToAsynTraceLevel(level);
        asynPrint(pasynUser, traceLevel, "%s\n", logMessage);
    }
}

/** 타임스탬프 문자열 생성 */
void ErrorHandler::getTimestampString(char* buffer, size_t bufferSize)
{
    if (buffer == NULL || bufferSize == 0) {
        return;
    }
    
    epicsTimeStamp currentTime;
    epicsTimeGetCurrent(&currentTime);
    
    // EPICS 시간을 문자열로 변환
    char epicsTimeStr[64];
    epicsTimeToStrftime(epicsTimeStr, sizeof(epicsTimeStr), "%Y-%m-%d %H:%M:%S.%06f", &currentTime);
    
    strncpy(buffer, epicsTimeStr, bufferSize - 1);
    buffer[bufferSize - 1] = '\0';
}

/** asyn 트레이스 레벨 변환 */
int ErrorHandler::convertToAsynTraceLevel(ErrorLevel level)
{
    switch (level) {
        case INFO:    return ASYN_TRACE_FLOW;
        case WARNING: return ASYN_TRACE_WARNING;
        case ERROR:   return ASYN_TRACE_ERROR;
        case FATAL:   return ASYN_TRACE_ERROR;
        default:      return ASYN_TRACE_ERROR;
    }
}