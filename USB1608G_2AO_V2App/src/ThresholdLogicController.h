#ifndef ThresholdLogicControllerInclude
#define ThresholdLogicControllerInclude

#include <asynPortDriver.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <shareLib.h>
#include "ErrorHandler.h"

/** 임계값 기반 로직 제어를 위한 asynPortDriver 클래스
 * 
 * 이 클래스는 아날로그 입력 값을 모니터링하고 설정된 임계값과 비교하여
 * 디지털 출력을 제어하는 기능을 제공합니다.
 * 히스테리시스 기능을 포함하여 안정적인 출력 제어를 보장합니다.
 */
class epicsShareClass ThresholdLogicController : public asynPortDriver, public epicsThreadRunable {
public:
    /** 생성자
     * \param[in] portName 이 드라이버의 asyn 포트 이름
     * \param[in] devicePort 연결할 장치 포트 이름
     * \param[in] deviceAddr 장치 주소
     */
    ThresholdLogicController(const char* portName, const char* devicePort, int deviceAddr);
    
    /** 소멸자 */
    virtual ~ThresholdLogicController();
    
    // asynPortDriver에서 상속받은 메서드들
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
    
    /** 임계값 로직 처리 메서드 */
    void processThresholdLogic();
    
    /** 모니터링 시작 */
    void startMonitoring();
    
    /** 모니터링 중지 */
    void stopMonitoring();
    
    /** 정적 스레드 함수 (epicsThread에서 호출) */
    static void monitorThreadFunc(void* param);
    
    /** epicsThreadRunable 인터페이스 구현 */
    virtual void run();
    
    // 테스트용 public 접근자 메서드들
    /** 테스트용: 매개변수 인덱스 접근자 */
    int getThresholdValueParam() const { return P_ThresholdValue; }
    int getCurrentValueParam() const { return P_CurrentValue; }
    int getOutputStateParam() const { return P_OutputState; }
    int getEnableParam() const { return P_Enable; }
    int getHysteresisParam() const { return P_Hysteresis; }
    int getUpdateRateParam() const { return P_UpdateRate; }
    int getAlarmStatusParam() const { return P_AlarmStatus; }

protected:
    // 매개변수 인덱스들
    int P_ThresholdValue;      ///< 임계값 설정 매개변수
    int P_CurrentValue;        ///< 현재 측정값 매개변수
    int P_OutputState;         ///< 출력 상태 매개변수
    int P_Enable;              ///< 활성화 상태 매개변수
    int P_Hysteresis;          ///< 히스테리시스 값 매개변수
    int P_UpdateRate;          ///< 업데이트 주기 매개변수
    int P_AlarmStatus;         ///< 알람 상태 매개변수
    int P_DevicePort;          ///< 장치 포트 이름 매개변수
    int P_DeviceAddr;          ///< 장치 주소 매개변수

private:
    // 스레드 관리
    epicsThread *monitorThread_;    ///< 모니터링 스레드 포인터
    bool threadRunning_;            ///< 스레드 실행 상태
    bool threadExit_;               ///< 스레드 종료 플래그
    
    // 임계값 로직 상태 변수들
    double thresholdValue_;         ///< 현재 임계값
    double currentValue_;           ///< 현재 측정값
    bool outputState_;              ///< 현재 출력 상태
    bool enabled_;                  ///< 활성화 상태
    double hysteresis_;             ///< 히스테리시스 값
    double updateRate_;             ///< 업데이트 주기 (Hz)
    int alarmStatus_;               ///< 알람 상태
    
    // 장치 연결 정보
    char devicePortName_[64];       ///< 연결할 장치 포트 이름
    int deviceAddr_;                ///< 장치 주소
    
    // 상태 추적
    epicsTimeStamp lastUpdate_;     ///< 마지막 업데이트 시간
    bool lastOutputState_;          ///< 이전 출력 상태 (상태 변화 감지용)
    
    // 내부 메서드들
    /** 장치에서 현재 값을 읽어옴 */
    asynStatus readCurrentValueFromDevice();
    
    /** 장치에 출력 상태를 설정 */
    asynStatus writeOutputStateToDevice(bool state);
    
    /** 알람 상태 업데이트 */
    void updateAlarmStatus();
    
    /** 매개변수 유효성 검사 */
    bool validateParameters();
    
    /** 오류 로깅 (ErrorHandler 사용) */
    void logError(const char* functionName, const char* message);
    
    /** 구성 유효성 검사 (ErrorHandler 사용) */
    bool validateConfigurationWithErrorHandler();
};

// IOC 쉘 명령어 등록 함수
extern "C" {
    epicsShareFunc int ThresholdLogicConfig(const char* portName, const char* devicePort, int deviceAddr);
    epicsShareFunc void ThresholdLogicHelp(void);
    epicsShareFunc void ThresholdLogicRegister(void);
}

// 매개변수 문자열 정의
#define THRESHOLD_VALUE_STRING      "THRESHOLD_VALUE"
#define CURRENT_VALUE_STRING        "CURRENT_VALUE"
#define OUTPUT_STATE_STRING         "OUTPUT_STATE"
#define ENABLE_STRING               "ENABLE"
#define HYSTERESIS_STRING           "HYSTERESIS"
#define UPDATE_RATE_STRING          "UPDATE_RATE"
#define ALARM_STATUS_STRING         "ALARM_STATUS"
#define DEVICE_PORT_STRING          "DEVICE_PORT"
#define DEVICE_ADDR_STRING          "DEVICE_ADDR"

#endif /* ThresholdLogicControllerInclude */