#ifndef ErrorHandlerInclude
#define ErrorHandlerInclude

#include <epicsTime.h>
#include <asynDriver.h>
#include <shareLib.h>

/** 오류 처리 및 로깅을 위한 유틸리티 클래스
 * 
 * 이 클래스는 EPICS IOC 애플리케이션에서 발생하는 다양한 오류를 
 * 분류하고 로깅하며, EPICS 알람 시스템과 통합하여 상태를 보고합니다.
 */
class epicsShareClass ErrorHandler {
public:
    /** 오류 레벨 정의 */
    enum ErrorLevel {
        INFO = 0,       ///< 정보성 메시지
        WARNING = 1,    ///< 경고 - 동작에 영향 없음
        ERROR = 2,      ///< 오류 - 기능에 영향 있음
        FATAL = 3       ///< 치명적 오류 - 시스템 중단 필요
    };
    
    /** EPICS 알람 심각도 정의 */
    enum AlarmSeverity {
        NO_ALARM = 0,       ///< 알람 없음
        MINOR_ALARM = 1,    ///< 경미한 알람
        MAJOR_ALARM = 2,    ///< 주요 알람
        INVALID_ALARM = 3   ///< 유효하지 않은 상태
    };
    
    /** EPICS 알람 상태 정의 */
    enum AlarmStatus {
        NO_ALARM_STATUS = 0,    ///< 정상 상태
        READ_ALARM = 1,         ///< 읽기 오류
        WRITE_ALARM = 2,        ///< 쓰기 오류
        HIHI_ALARM = 3,         ///< 상한 상한 알람
        HIGH_ALARM = 4,         ///< 상한 알람
        LOLO_ALARM = 5,         ///< 하한 하한 알람
        LOW_ALARM = 6,          ///< 하한 알람
        STATE_ALARM = 7,        ///< 상태 알람
        COS_ALARM = 8,          ///< 변화 알람
        COMM_ALARM = 9,         ///< 통신 알람
        TIMEOUT_ALARM = 10,     ///< 타임아웃 알람
        HW_LIMIT_ALARM = 11,    ///< 하드웨어 제한 알람
        CALC_ALARM = 12,        ///< 계산 오류 알람
        SCAN_ALARM = 13,        ///< 스캔 오류 알람
        LINK_ALARM = 14,        ///< 링크 오류 알람
        SOFT_ALARM = 15,        ///< 소프트웨어 알람
        BAD_SUB_ALARM = 16,     ///< 잘못된 서브레코드 알람
        UDF_ALARM = 17,         ///< 정의되지 않은 값 알람
        DISABLE_ALARM = 18,     ///< 비활성화 알람
        SIMM_ALARM = 19,        ///< 시뮬레이션 알람
        READ_ACCESS_ALARM = 20, ///< 읽기 접근 알람
        WRITE_ACCESS_ALARM = 21 ///< 쓰기 접근 알람
    };
    
    /** 구성 유효성 검사 결과 */
    struct ValidationResult {
        bool isValid;               ///< 유효성 검사 통과 여부
        ErrorLevel errorLevel;      ///< 오류 레벨
        char errorMessage[256];     ///< 오류 메시지
        char suggestion[256];       ///< 해결 방안 제안
    };
    
    /** ThresholdLogicController 구성 매개변수 */
    struct ThresholdConfig {
        char portName[64];          ///< 포트 이름
        char devicePort[64];        ///< 장치 포트 이름
        int deviceAddr;             ///< 장치 주소
        double updateRate;          ///< 업데이트 주기 (Hz)
        int priority;               ///< 스레드 우선순위
        double thresholdValue;      ///< 임계값
        double hysteresis;          ///< 히스테리시스 값
    };

public:
    /** 오류 로깅 메서드
     * \param[in] level 오류 레벨
     * \param[in] source 오류 발생 소스 (클래스명::메서드명)
     * \param[in] message 오류 메시지
     * \param[in] pasynUser asyn 사용자 포인터 (선택사항)
     */
    static void logError(ErrorLevel level, const char* source, const char* message, 
                        asynUser* pasynUser = NULL);
    
    /** 상세 오류 로깅 메서드 (추가 정보 포함)
     * \param[in] level 오류 레벨
     * \param[in] source 오류 발생 소스
     * \param[in] message 오류 메시지
     * \param[in] details 상세 정보
     * \param[in] errorCode 오류 코드
     * \param[in] pasynUser asyn 사용자 포인터 (선택사항)
     */
    static void logDetailedError(ErrorLevel level, const char* source, const char* message,
                               const char* details, int errorCode, asynUser* pasynUser = NULL);
    
    /** EPICS 알람 상태 설정
     * \param[in] pasynUser asyn 사용자 포인터
     * \param[in] status 알람 상태
     * \param[in] severity 알람 심각도
     * \return asynStatus 결과
     */
    static asynStatus setAlarmStatus(asynUser* pasynUser, AlarmStatus status, AlarmSeverity severity);
    
    /** ThresholdLogicController 구성 유효성 검사
     * \param[in] config 검사할 구성
     * \return ValidationResult 검사 결과
     */
    static ValidationResult validateConfiguration(const ThresholdConfig& config);
    
    /** 런타임 오류 처리
     * \param[in] source 오류 발생 소스
     * \param[in] errorType 오류 유형
     * \param[in] errorCode 오류 코드
     * \param[in] pasynUser asyn 사용자 포인터 (선택사항)
     * \return 복구 가능 여부
     */
    static bool handleRuntimeError(const char* source, const char* errorType, 
                                 int errorCode, asynUser* pasynUser = NULL);
    
    /** 통신 오류 처리
     * \param[in] source 오류 발생 소스
     * \param[in] devicePort 장치 포트 이름
     * \param[in] deviceAddr 장치 주소
     * \param[in] operation 수행 중이던 작업
     * \param[in] pasynUser asyn 사용자 포인터 (선택사항)
     * \return 재시도 권장 여부
     */
    static bool handleCommunicationError(const char* source, const char* devicePort,
                                       int deviceAddr, const char* operation,
                                       asynUser* pasynUser = NULL);
    
    /** 스레드 오류 처리
     * \param[in] source 오류 발생 소스
     * \param[in] threadName 스레드 이름
     * \param[in] errorMessage 오류 메시지
     * \param[in] pasynUser asyn 사용자 포인터 (선택사항)
     * \return 스레드 재시작 권장 여부
     */
    static bool handleThreadError(const char* source, const char* threadName,
                                const char* errorMessage, asynUser* pasynUser = NULL);
    
    /** 매개변수 유효성 검사
     * \param[in] paramName 매개변수 이름
     * \param[in] value 검사할 값
     * \param[in] minValue 최소값
     * \param[in] maxValue 최대값
     * \param[in] source 호출 소스
     * \return 유효성 검사 통과 여부
     */
    static bool validateParameter(const char* paramName, double value, 
                                double minValue, double maxValue, const char* source);
    
    /** 정수 매개변수 유효성 검사
     * \param[in] paramName 매개변수 이름
     * \param[in] value 검사할 값
     * \param[in] minValue 최소값
     * \param[in] maxValue 최대값
     * \param[in] source 호출 소스
     * \return 유효성 검사 통과 여부
     */
    static bool validateIntParameter(const char* paramName, int value, 
                                   int minValue, int maxValue, const char* source);
    
    /** 문자열 매개변수 유효성 검사
     * \param[in] paramName 매개변수 이름
     * \param[in] value 검사할 문자열
     * \param[in] maxLength 최대 길이
     * \param[in] allowEmpty 빈 문자열 허용 여부
     * \param[in] source 호출 소스
     * \return 유효성 검사 통과 여부
     */
    static bool validateStringParameter(const char* paramName, const char* value,
                                      size_t maxLength, bool allowEmpty, const char* source);
    
    /** 오류 통계 정보 가져오기
     * \param[out] infoCount 정보 메시지 수
     * \param[out] warningCount 경고 메시지 수
     * \param[out] errorCount 오류 메시지 수
     * \param[out] fatalCount 치명적 오류 메시지 수
     */
    static void getErrorStatistics(int* infoCount, int* warningCount, 
                                 int* errorCount, int* fatalCount);
    
    /** 오류 통계 초기화 */
    static void resetErrorStatistics();
    
    /** 오류 레벨을 문자열로 변환
     * \param[in] level 오류 레벨
     * \return 오류 레벨 문자열
     */
    static const char* errorLevelToString(ErrorLevel level);
    
    /** 알람 심각도를 문자열로 변환
     * \param[in] severity 알람 심각도
     * \return 알람 심각도 문자열
     */
    static const char* alarmSeverityToString(AlarmSeverity severity);
    
    /** 알람 상태를 문자열로 변환
     * \param[in] status 알람 상태
     * \return 알람 상태 문자열
     */
    static const char* alarmStatusToString(AlarmStatus status);

private:
    // 정적 멤버 변수들 (오류 통계)
    static int infoCount_;      ///< 정보 메시지 카운터
    static int warningCount_;   ///< 경고 메시지 카운터
    static int errorCount_;     ///< 오류 메시지 카운터
    static int fatalCount_;     ///< 치명적 오류 메시지 카운터
    
    /** 내부 로깅 메서드
     * \param[in] level 오류 레벨
     * \param[in] source 오류 발생 소스
     * \param[in] message 메시지
     * \param[in] pasynUser asyn 사용자 포인터
     */
    static void internalLog(ErrorLevel level, const char* source, const char* message,
                          asynUser* pasynUser);
    
    /** 타임스탬프 문자열 생성
     * \param[out] buffer 출력 버퍼
     * \param[in] bufferSize 버퍼 크기
     */
    static void getTimestampString(char* buffer, size_t bufferSize);
    
    /** asyn 트레이스 레벨 변환
     * \param[in] level ErrorHandler 오류 레벨
     * \return asyn 트레이스 레벨
     */
    static int convertToAsynTraceLevel(ErrorLevel level);
};

#endif /* ErrorHandlerInclude */