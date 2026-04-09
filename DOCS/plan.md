# DCAS-PolicyEngine 상세 구현 계획 (Research 기반 재작성, 2026-04-09)

본 문서는 `DOCS/research.md`를 기준으로 작성한 **실행 계획 문서**다.  
범위는 “어떻게 만들지”이며, **실제 구현은 포함하지 않는다**.

---

## 0. 전제 및 고정 조건

- 구현 언어: **C++ 우선**
- 통신: **ZMQ 우선** (기존 ads-skynet 포트/패턴 정합)
- 역할 분리:
  - 인지팀: 물리/인지 신호 산출
  - 정책팀(본 레포): 상태 전이 + 정책 판단 + 제한 명령 산출
  - HMI팀: 대시보드/표현
- 사용자 지시 반영:
  - `hands_on`은 정책 입력에서 제외
  - 현재 차량에는 실제 브레이크 하드웨어가 없으므로 `brake_request`는 논리 신호/대체 동작으로 설계

---

## 1. 접근 방식 (Architecture Approach)

## 1.1 정책 엔진 책임

정책 엔진은 다음 4단계를 단일 tick 파이프라인으로 수행한다.

1. 입력 수신/검증 (`dms.input.v1`)
2. 상태 전이 계산 (`DriverState`, `DcasState`)
3. 정책 매핑 (`reason + state + confidence -> actions`)
4. 제한 명령 출력 (`dcas.state.v1`, `lkas.limit.v1`)

## 1.2 시스템 경계

- 정책 엔진은 “인지 결과”를 입력받고 “제어 제한 의도”를 출력한다.
- LKAS 코어 알고리즘(PD/PID/MPC)은 **그대로 유지**한다.
- DCAS는 **독립 중재 프로세스**로서 LKAS output shared memory(입력 채널)에서 control 값을 읽어 제한/보정 후, DCAS output shared memory(출력 채널)에 기록한다.
- 차량 실행 모듈(`Vehicle-jetracer`)은 **무수정**으로 유지하며, DCAS output control channel을 읽어 최종 actuation만 수행한다.
- 제어 흐름은 다음 순서를 따른다.
  - `LKAS control values -> shared memory -> DCAS -> DCAS control values -> shared memory -> Vehicle-jetracer`
  - 즉, LKAS가 만든 제어값을 정책 엔진이 한 번 더 읽고, DCAS 제한값을 다시 내려보내 최종 차량 실행에 반영한다.
- PC(Router)는 인지/대시보드/사용자 인터랙션 계층이며, Jetson 내부의 LKAS/DCAS 실행 경로와는 분리된다.

## 1.3 왜 C++ + ZMQ인가

- C++: 장기적으로 실시간성/성능/자원 제약 대응에 유리
- ZMQ: 현재 레포군(`common`, `lkas`, `Web-viewer`, `Vehicle-jetracer`)과 직접 정합
- 결론: “신규 정책 코어는 C++”, “주변 시스템과 연결은 ZMQ 계약 고정”

---

## 2. 데모 시나리오 기반 런타임 흐름

아래는 실제 데모 동작 순서에 파일 책임을 대입한 흐름이다.

### 2.1 Step A: 노트북 -> 정책엔진 입력

- PC(Router)에서 생성되는 인지팀 송신 데이터(예):
  - `eyes_on`, `driver_present`, `pose_confidence`
  - `vlm.reason`, `vlm.confidence`, `parse_ok`, `latency_ms`
- LKAS 송신 데이터(예):
  - `curvature`, `throttle`, `steer`
- 정책팀 수신:
  - Jetson 쪽 ZMQ Subscriber 또는 shared memory reader가 `dms.input.v1`를 수신
  - JSON 스키마/필수 필드 검증
  - Jetson 내부 LKAS shared memory/ZMQ 경로에서 현재 제어 상태도 함께 수신

### 2.2 Step B: 상태머신 전이

- 타이머 누적:
  - `eyes_off_elapsed`
  - `driver_absent_elapsed`
  - `warning_elapsed`
- 전이:
  - `OK -> WARNING -> UNRESPONSIVE -> ABSENT`
  - 히스테리시스/복귀 지연 적용

### 2.3 Step C: 정책 판단

- 입력: `driver_state`, `dcas_state`, `reason`, `confidence`, `vehicle_speed`, `road_curvature`, `lkas_throttle`, `lkas_steer`
- 출력:
  - HMI 액션(`HMI_WARN`, `BEEP_LEVEL_UP`)
  - 제어 제한(`throttle_limit`, `steer_limit`)
  - 긴급 플래그(`rmf_request`)

추가 원칙:

- 속도가 높거나 커브가 큰 구간에서는 동일한 `driver_state`라도 더 보수적인 제한값을 적용한다.
- LKAS의 현재 `throttle`, `steer`, `curvature`는 “현재 주행 맥락”으로 사용하고, 정책 판단 시 경고 강도/제한값/긴급 전환에 반영한다.

### 2.4 Step D: LKAS 제한 적용

- 정책엔진은 `lkas.limit.v1` 메시지를 publish
- 정책엔진은 LKAS로부터 읽은 `curvature`, `throttle`, `steer`를 기준으로 제한값을 재계산한다.
- 정책엔진은 재계산된 제한값을 DCAS output shared memory에 기록하고, vehicle-jetracer는 해당 채널을 읽는다.
- LKAS 자체는 수정하지 않으며, DCAS는 LKAS output 채널을 읽고 DCAS output 채널에 최종값을 기록하는 방식으로 개입한다.
  - `steering = clamp(steering, -steer_limit, +steer_limit)`
  - `throttle = min(throttle, throttle_limit)`
  - `if emergency: throttle = 0` (브레이크 미장착 대체)
- `Vehicle-jetracer`는 수정하지 않으며, DCAS가 덮어쓴 최종 control 값을 그대로 읽어 actuation 한다.

### 2.5 Step E: 대시보드 송신

- 정책엔진이 `dcas.state.v1`를 ZMQ publish
- `Web-viewer`가 수신 후 WebSocket으로 브라우저 전달

---

## 3. 파일/디렉토리 계획 (C++ 기준)

```text
DCAS-PolicyEngine/
  CMakeLists.txt
  third_party/
  src/
    main.cpp
    config/
      ports.hpp
      thresholds.hpp
    model/
      input.hpp
      output.hpp
      enums.hpp
    io/
      zmq_subscriber.cpp
      zmq_subscriber.hpp
      zmq_publisher.cpp
      zmq_publisher.hpp
      json_codec.cpp
      json_codec.hpp
    engine/
      state_machine.cpp
      state_machine.hpp
      evaluator.cpp
      evaluator.hpp
      timers.cpp
      timers.hpp
    policy/
      rules.cpp
      rules.hpp
      mapper.cpp
      mapper.hpp
    integration/
      lkas_limit_adapter.cpp
      lkas_limit_adapter.hpp
      hmi_adapter.cpp
      hmi_adapter.hpp
    observability/
      event_logger.cpp
      event_logger.hpp
  tests/
    test_state_machine.cpp
    test_policy_mapper.cpp
    test_timeout_fallback.cpp
    test_json_contract.cpp
  DOCS/
    research.md
    plan.md
    interface-spec-v1.md
    state-machine-v1.md
```

### 3.1 각 파일/디렉토리 역할

- `CMakeLists.txt`: 빌드 타깃/의존성/테스트 타깃 정의
- `third_party/`: 외부 라이브러리(예: JSON, 테스트 프레임워크) 관리
- `src/main.cpp`: 프로세스 진입점, 루프/종료 시그널/초기화 오케스트레이션

- `src/config/ports.hpp`: ZMQ 엔드포인트, 토픽, 채널명 상수
- `src/config/thresholds.hpp`: 상태 전이 임계값, 히스테리시스, timeout 상수

- `src/model/input.hpp`: `dms.input.v1` 입력 구조체
- `src/model/output.hpp`: `dcas.state.v1`, `lkas.limit.v1` 출력 구조체
- `src/model/enums.hpp`: `DriverState`, `DcasState`, `ReasonCode` 열거형

- `src/io/zmq_subscriber.*`: 인지 입력 + LKAS 제어 상태 수신
- `src/io/zmq_publisher.*`: 정책 결과/HMI/LKAS 제한 송신
- `src/io/json_codec.*`: JSON 직렬화/역직렬화, 스키마 검증/기본값 처리

- `src/engine/state_machine.*`: 상태 전이 규칙 적용
- `src/engine/timers.*`: elapsed 계산, timer reset/hold 관리
- `src/engine/evaluator.*`: 단일 tick 파이프라인(인지 입력 + LKAS control 상태 + 정책→출력) 총괄

- `src/policy/rules.*`: 정책 테이블(상태+이유+상황 변수에 따른 제한값)
- `src/policy/mapper.*`: 룰 조회/우선순위/충돌 해소 로직

- `src/integration/lkas_limit_adapter.*`: `lkas.limit.v1` 생성/송신 포맷 어댑터
- `src/integration/hmi_adapter.*`: HMI 문구/레벨/카운트다운 생성 어댑터
- `src/integration/control_channel_adapter.*`: LKAS output SHM 입력 + DCAS output SHM 기록을 담당하는 어댑터

- `src/observability/event_logger.*`: 상태 전이/근거/타이머/confidence 구조화 로그

- `tests/test_state_machine.cpp`: 전이/히스테리시스/경계값 테스트
- `tests/test_policy_mapper.cpp`: reason/상황 변수 기반 정책 매핑 테스트
- `tests/test_timeout_fallback.cpp`: stale 입력/누락/fallback 테스트
- `tests/test_json_contract.cpp`: 입출력 계약(스키마) 호환성 테스트

- `DOCS/research.md`: 요구사항/근거/아키텍처 리서치
- `DOCS/plan.md`: 구현 계획 및 파일 책임 정의
- `DOCS/interface-spec-v1.md`: 토픽/JSON 계약 문서
- `DOCS/state-machine-v1.md`: 전이표, 임계값, 예외 규칙 문서

---

## 4. 수정될 파일 경로 (타 레포 포함)

## 4.1 본 레포 (`DCAS-PolicyEngine`)

- 신규 생성(계획):
  - `CMakeLists.txt`
  - `src/**` 전체
  - `tests/**` 전체
  - `DOCS/interface-spec-v1.md`
  - `DOCS/state-machine-v1.md`

## 4.2 연동 레포 (최소 수정 계획)

- `lkas` / `Vehicle-jetracer`
  - **수정 없음**
  - 기존 shared memory / control channel 계약만 사용(DCAS가 중간 SHM 브리지 역할 수행)

## 4.3 본 레포의 개입 방식

- `DCAS-PolicyEngine`이 LKAS output shared memory 채널에서 control 값을 읽는다.
- DCAS가 제한/보정한 최종 control 값을 DCAS output shared memory 채널에 기록한다.
- `Vehicle-jetracer`는 수정 없이 DCAS output 채널을 읽어 actuation 한다.

---

## 5. 핵심 코드 스니펫 (설계 예시, 구현 아님)

### 5.1 입력 모델 예시 (`src/model/input.hpp`)

```cpp
struct VlmInput {
  bool called;
  std::string reason;
  float confidence;
  bool parse_ok;
  int latency_ms;
};

struct DmsInput {
  double ts;
  uint64_t seq;
  bool eyes_on;
  bool driver_present;
  float pose_confidence;
  VlmInput vlm;
};
```

### 5.2 상태머신 인터페이스 예시 (`src/engine/state_machine.hpp`)

```cpp
enum class DriverState { OK, WARNING, UNRESPONSIVE, ABSENT };

class StateMachine {
 public:
  DriverState Tick(const DmsInput& input);
 private:
  double eyes_off_start_ts_ = -1.0;
  double absent_start_ts_ = -1.0;
  double warning_start_ts_ = -1.0;
};
```

### 5.3 LKAS 제한 적용 예시 (`lkas` 측 개념 스니펫)

```python
# conceptual pseudo-code only
control = controller.process_detection(detection)
limit = dcas_limit_subscriber.latest()  # throttle_limit, steer_limit, emergency

control.steering = max(-limit.steer_limit, min(limit.steer_limit, control.steering))
control.throttle = min(limit.throttle_limit, control.throttle)
if limit.emergency:
    control.throttle = 0.0
```

### 5.4 출력 메시지 예시 (`dcas.state.v1`)

```json
{
  "ts": 1712500000.223,
  "driver_state": "WARNING",
  "dcas_state": "RESTRICTED",
  "actions": ["HMI_WARN", "THROTTLE_LIMIT"],
  "limits": {
    "throttle_limit": 0.08,
    "steer_limit": 0.20,
    "emergency": false
  },
  "reason_codes": ["eyes_off_timeout", "vlm_phone"]
}
```

---

## 6. 인터페이스 계약 (v1)

## 6.1 입력: `dms.input.v1`

- 필수: `ts`, `seq`, `eyes_on`, `driver_present`, `pose_confidence`
- 선택: `vlm` 블록 (`reason`, `confidence`, `parse_ok`, `latency_ms`)
- 금지/미사용: `hands_on` (현재 정책 버전에서 평가 제외)

## 6.1.1 LKAS 상태 입력: `lkas.control.v1`

- 필수: `ts`, `seq`, `curvature`, `throttle`, `steer`
- 사용 목적: 정책 판단 시 현재 주행 맥락 반영
- 해석:
  - `curvature`: 곡률이 클수록 제한값을 더 보수적으로
  - `throttle`: 현재 속도/가속 여유 추정에 활용
  - `steer`: 현재 조향 부담/곡선 진입 여부 추정에 활용

## 6.2 출력: `dcas.state.v1`

- 필수: `driver_state`, `dcas_state`, `actions`, `reason_codes`
- 제어제한: `limits.throttle_limit`, `limits.steer_limit`, `limits.emergency`

## 6.2.1 LKAS 제한 출력: `lkas.limit.v1`

- 필수: `seq`, `throttle_limit`, `steer_limit`, `emergency`, `ttl_ms`
- 추가 필드(선택): `source_curvature`, `source_throttle`, `source_steer`
- 목적: LKAS가 만든 현재 주행 상태를 반영하여 제한값을 더 정교하게 적용

## 6.3 LKAS 제한 채널: `lkas.limit.v1`

- 필수: `seq`, `throttle_limit`, `steer_limit`, `emergency`, `ttl_ms`
- 적용 정책: TTL 만료 시 LKAS는 마지막 제한값 무효화 또는 fail-safe fallback

---

## 7. 트레이드오프 분석

## 7.1 C++ vs Python

- C++ 장점: 성능/예측가능성/장기 포팅성
- C++ 단점: 초기 개발 속도 저하, 빌드/테스트 인프라 직접 구축 필요
- Python 장점: 빠른 실험/개발
- Python 단점: 장기 실시간 경로에서 제약 가능

결론: 본 프로젝트는 “재포팅 없이 제대로” 요구가 있으므로 C++ 우선이 전략적으로 타당하다.

## 7.2 ZMQ vs ROS2/MQTT

- ZMQ 장점: 기존 스택 정합, brokerless, 구성 단순
- ROS2 장점: QoS/노드 생태계
- MQTT 장점: 외부 IoT 연동 용이

결론: 현재 데모/통합 목표는 ZMQ 우선, ROS2/MQTT는 확장 시점에 검토.

## 7.3 LKAS 개입 깊이

- Deep integration(코어 수정): 유연성 높음, 리스크 높음
- Limit-layer integration(권장): 변경 최소, 검증 용이

---

## 8. 검증 계획 (구현 전 설계 수준)

## 8.1 상태머신 테스트

- 정상 전이: `OK -> WARNING -> UNRESPONSIVE -> ABSENT`
- 복귀 전이: 히스테리시스 조건 만족 시 단계적 복귀
- 경계값: 임계값 직전/직후, 신뢰도 저하 시 fallback

## 8.2 통신/계약 테스트

- JSON 스키마 유효성
- 필드 누락/타입 오류 처리
- ZMQ 메시지 지연/유실/역순 도착 대응

## 8.3 LKAS 제한 테스트

- throttle/steer clamp 정상 적용
- emergency 시 throttle 0 강제
- TTL 만료 시 제한 무효 처리
- curvature/throttle/steer 입력에 따라 제한값이 더 보수적으로 바뀌는지 확인

---

## 9. 단계별 실행 로드맵 (구현 착수 전 기준)

### Phase 1: 계약/문서 고정 (1주)

- `interface-spec-v1.md`, `state-machine-v1.md` 확정
- 포트/토픽/메시지 버전 동결

### Phase 2: 엔진 골격 구현 계획 확정 (2~3주)

- CMake/디렉토리 구조/테스트 전략 확정
- 인지팀/LKAS팀과 인터페이스 리허설

### Phase 3: 통합 계획 확정 (3~5주)

- LKAS limit-layer 개입 지점 확정
- 대시보드 송신 필드 최종 확정

### Phase 4: 검증 시나리오 확정 (5~7주)

- 데모 시나리오(away/drowsy/phone)별 기대 전이/행동 표준화

---

## 10. 빠뜨리기 쉬운 추가 체크리스트 (기타 내용)

- 스키마 버전 호환 정책(`v1 -> v1.1`)
- 메시지 타임스탬프 기준(clock drift) 합의
- stale VLM 처리 기준(예: `latency_ms > threshold` 무시)
- 로깅 표준(전이 원인, confidence, timer snapshot)
- fail-open/fail-safe 정책(입력 단절 시 보수 모드)
- 브레이크 미장착 제약에 따른 대체 안전 전략 문서화

---

> 이 문서는 구현 지시가 아니라 구현 준비를 위한 상세 설계 계획서다.  
> 코드 작성/레포 변경은 별도 승인 후 진행한다.
