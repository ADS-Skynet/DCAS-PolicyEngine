# DCAS 프로젝트/역할 심층 리서치 (2026-04-08)

본 문서는 다음을 기반으로 작성했다.

- 핵심 내부 문서 완독: `DOCS/last project.md` (전체 라인 읽음)
- 내부 레포 조사: `common`, `lkas`, `Vehicle-jetracer`, `Web-viewer`, `DCAS-PolicyEngine`
- 외부 자료 조사: MediaPipe Pose Landmarker, ROS2 QoS, OPA(Policy-as-Code), NHTSA 자동화 안전 개요

---

## 1) 프로젝트의 본질: “차량 한계”가 아니라 “운전자 한계”를 다루는 DCAS

### 1.1 문제 프레이밍

현재 프로젝트의 핵심 전환은 다음과 같다.

- 기존 초점: LKAS/L2 제어 성능 중심 (차량이 차선을 잘 유지하는가)
- 현재 초점: DCAS/R171 맥락의 운전자 참여(engagement) 중심

즉, 제어 알고리즘만 잘 돌아가는 시스템이 아니라,

- 운전자 상태를 감시하고
- 오용/과신을 줄이며
- 단계적 경고/제한/감속/정지로 이어지는
- 감독형(L2+) 안전 상호작용 시스템

을 설계/구현하는 것이 본 과제의 본질이다.

### 1.2 왜 이 주제가 지금 유효한가

`last project.md`의 정리와 시장/정책 흐름을 합치면:

- L3 완전자동화보다, 단기~중기 양산은 L2+/감독형이 더 광범위하게 확산
- 따라서 “운전자가 실제로 감독하고 있는가”가 제품 신뢰성과 규제 대응의 중심
- 이 때문에 DMS/인캐빈 모니터링/engagement 로직은 단순 부가 기능이 아니라 핵심 기능

프로젝트 가치 포인트는 “신규 센서”가 아니라,

1. 운전자 맥락 인지(1차 CV + 2차 VLM),
2. 그 결과를 정책 엔진으로 정형화,
3. 안전 제어 계층으로 연결,

하는 end-to-end 통합 능력에 있다.

---

## 2) 현재 ads-skynet 생태계 기준 기술 현황

## 2.1 이미 잘 갖춰진 기반 (활용 가능 자산)

### 공통 레이어 (`common`)

- 타입/모델/시각화/통신 유틸이 이미 분리되어 있음
- `common/src/config/config.py`에 통신 포트/채널/타이밍/스트리밍 파라미터가 표준화됨
- Shared memory + ZMQ를 동시에 지원하는 운영 패턴이 이미 정착

핵심 포트(기본값):

- `5557`: Viewer broadcast
- `5558`: Viewer action
- `5559`: Parameter update
- `5560`: Param forward to servers
- `5561`: Action forward
- `5562`: Vehicle status

### LKAS 레이어 (`lkas`)

- 검출/의사결정/통합이 이미 모듈화
- `integration/README.md` 기준 SHM + ZMQ 이중 구조가 문서화되어 있음
- 브로커 기반 중앙 조정(`LKASBroker`) 패턴이 존재

### 차량 실행 레이어 (`Vehicle-jetracer`)

- 차량 루프와 상태 publish 경로가 존재
- `common.config`를 받아 통신/제어 파라미터를 주입하는 구조가 이미 있음

### HMI/모니터링 레이어 (`Web-viewer`)

- ZMQ 수신 + WebSocket 송신 구조 완성도 높음
- 대시보드/실시간 상태 표출/액션 전송 패턴이 구현됨

요약하면, “통신/시각화/제어 골격”은 이미 있으므로 DCAS-PolicyEngine은

- 정책 로직,
- 상태 머신,
- DMS/VLM 입력 정규화,

에 집중하면 된다.

## 2.2 현재 DCAS-PolicyEngine 상태

- `DCAS-PolicyEngine`은 현재 README만 존재
- 즉, 아키텍처/정책/인터페이스를 처음부터 정의할 수 있는 타이밍
- 동시에 팀장의 설계 품질이 곧 레포 품질이 되는 상태

---

## 3) 당신의 역할 정의 (팀장 + 아키텍트 + 운전자 레벨 정책 설계자)

문서 맥락을 종합하면, 역할은 단순 개발자가 아니라 “시스템 책임 계층”이다.

## 3.1 팀장 관점 책임

- 요구사항의 단일 소스(Single Source of Truth) 유지
- 타 팀(DMS, LKAS, VCP-G) 인터페이스 계약 확정
- 데모 시나리오 및 평가 기준(KPI) 정의
- 기술 리스크/일정 리스크를 초기에 가시화

## 3.2 아키텍트 관점 책임

- Perception(1차/2차)과 Policy, Control 계층을 느슨 결합
- “결정(Decision)”과 “집행(Enforcement)” 분리
- 실패 모드에서의 degrade path 설계

## 3.3 운전자 레벨/맥락 대응 로직 설계자 책임

- 운전자 engagement 상태를 단계화하고
- 맥락(reason)을 정책으로 매핑하고
- 경고/제한/감속/정지 시퀀스를 타이머 기반으로 일관 적용

핵심 산출물은 코드 그 자체보다,

1. 상태 정의,
2. 전이 규칙,
3. 임계값 근거,
4. 예외 처리,

를 모두 갖춘 “정책 실행 체계”다.

### 3.4 팀 분업 원칙 (합의안)

- 인지팀: 인지 정확도/신뢰도/품질 강화에 집중
   - 산출 책임: `hands_on`, `eyes_on`, `driver_present`, `pose_confidence`, `vlm.reason`, `vlm.confidence`, `parse_ok`, `latency_ms`
- 정책팀(DCAS-PolicyEngine): 판단 책임 확보
   - 산출 책임: `driver_state`, `dcas_state`, `actions`, `reason_codes`, 타이머/히스테리시스 기반 전이
- HMI팀(프론트엔드): 시각화/UX 설계 및 메시지 표현 담당
   - 산출 책임: 경고 단계별 UI, 메시지 표현, 이벤트 시각화

핵심 원칙: 인지팀은 “측정/추출”, 정책팀은 “판단/전이/집행”, HMI팀은 “표현/상호작용”에 집중한다.

---

## 4) 요구사항 정제 (R171 맥락 + 프로젝트 현실 제약)

`last project.md`의 핵심 요구사항을 구현 가능한 형태로 재정의:

### 4.1 기능 요구사항 (Must)

1. 운전자 참여 상태 평가
   - hands-on, eyes-on, driver-present를 입력으로 사용
2. 경고 단계 상승
   - 경고 → 강화 경고 → 제한 → RMF/MRM 요청
3. 맥락 기반 대응
   - VLM reason(phone, drowsy, away, talking, etc.)에 따른 대응 분기
4. HMI 설명형 메시지
   - “현재 상태 + 지금 해야 할 행동”을 항상 함께 제시

### 4.2 비기능 요구사항 (Must)

1. 실시간성
   - fast loop(저지연): MediaPipe/기초 DMS 신호
   - slow loop(고비용): VLM 이유 추론
2. 내결함성
   - VLM 응답 누락/지연/불량 JSON에도 정책 엔진이 안정 동작
3. 관측가능성
   - 상태 전이, 타이머, 트리거 이유를 로깅

### 4.3 현실 제약 (Project constraints)

- 2개월 내 시연
- VLM 지연 가능성 큼
- MCU fail-safe는 후순위 구현 가능성
- 데모 시각화 임팩트 필요

---

## 5) 권장 시스템 아키텍처 (현 레포와 정합)

## 5.1 계층 구조

### Layer A: Perception Input Layer

- MediaPipe/기초 DMS 출력: `hands_on`, `eyes_on`, `driver_present`, 신뢰도
- VLM 출력: `reason`, `confidence`, `raw_text`, `parse_ok`
- 주의: `driver_state` 최종 판정은 Layer B의 책임이며 Layer A는 물리/인지 신호를 제공한다.

### Layer B: Policy Engine Layer (DCAS-PolicyEngine 핵심)

- 입력 정규화/신뢰도 게이팅
- 상태 머신 계산
- 정책 결정(경고 강도, DCAS 제한 단계, 감속 목표)

### Layer C: Control Arbitration Layer

- LKAS 명령과 DCAS 안전 명령 arbitration
- 정책 출력 -> 차량 제어 명령(steer 제한, throttle 감쇠, brake 펄스)

### Layer D: HMI/Telemetry Layer

- GUI/Web-viewer로 상태, 이유, 카운트다운, 권고 메시지 전송
- 이벤트 로깅(사후 분석/발표 자료)

## 5.2 호스트 분담(현 문서 방향 유지 + 보완)

- 노트북:
  - 카메라 캡처, MediaPipe, VLM API 호출
  - DMS/VLM 결과 전송
  - 대시보드/HMI
- 젯슨:
  - DCAS 상태 머신 실행
  - 차량 제어 적용
  - heartbeat 송신
- MCU(후순위):
  - 젯슨 이상 시 최소 감속/정지 보장

---

## 6) 정책 엔진 설계 원칙 (핵심)

OPA의 일반 원칙(Decision-Policy decoupling)은 본 프로젝트에도 유효하다.

### 6.1 원칙

1. 정책과 실행 코드 분리
   - if/else 난립 대신 규칙 테이블/정책 모듈화
2. 입력/출력 계약 고정
   - 입력 JSON 스키마와 출력 액션 스키마를 버전관리
3. 결정 근거 반환
   - 단순 state만 아니라 `reasons[]`, `violations[]` 반환

### 6.2 권장 출력 형태

```json
{
  "driver_state": "WARNING",
  "dcas_state": "RESTRICTED",
  "actions": ["HMI_WARN", "BEEP_MEDIUM", "THROTTLE_LIMIT"],
  "reasons": ["eyes_off_timeout", "vlm_reason_phone"],
  "timers": {
    "eyes_off_elapsed": 4.1,
    "warning_elapsed": 6.2
  },
  "confidence": {
    "perception": 0.82,
    "vlm": 0.67
  }
}
```

이 구조를 쓰면 디버깅/발표/테스트가 모두 쉬워진다.

---

## 7) 권장 상태 머신 (운전자 상태 + DCAS 상태 이중화)

## 7.1 운전자 상태 (`DriverState`)

- `OK`
- `WARNING`
- `UNRESPONSIVE`
- `ABSENT`

진입 근거 예시:

- `WARNING`: hands-off 또는 eyes-off가 `T_warn` 초과
- `UNRESPONSIVE`: WARNING 이후 `T_response` 내 복귀 입력 없음
- `ABSENT`: driver_present false 지속 또는 UNRESPONSIVE 장기 지속

## 7.2 시스템 상태 (`DcasState`)

- `OFF`
- `ON_OK`
- `WARNING`
- `RESTRICTED`
- `RMF_REQUESTED` (또는 `MRM_REQUESTED`)

전이 원칙:

- 상태 상승은 빠르게, 상태 복귀는 보수적으로(히스테리시스)
- 단일 프레임 이벤트로 상태 급변 금지(시간 누적 기반)
- VLM 불확실 시 “기본 안전 정책” 적용

---

## 8) 맥락(reason) 기반 대응 매핑 (차별화 포인트)

`last project.md`의 아이디어를 정책 테이블로 정식화:

| reason | 기본 위험도 | 권장 대응 |
|---|---:|---|
| `phone` | High | 경고음 증가 + 짧은 제동 펄스 + 조향 보조 제한 준비 |
| `drowsy` | High | 장음 경고 + 점진 감속 + 휴식 권고 메시지 |
| `away` | Critical | 즉시 제한 + 강한 경고 + RMF/MRM 카운트다운 |
| `talking` | Medium | 시각+음성 경고, 지속 시 제한 |
| `eating/smoking` | Medium~High | 경고 및 집중 복귀 유도 |
| `unclear/none` | Low~Medium | 기본 경고 정책 유지, 오검출 방지 |

중요: reason은 “보조 근거”다.

- 1차 안전 트리거는 engagement 타이머(hands/eyes/present)
- reason은 대응 강도와 메시지 personalization에 사용

---

## 9) 통신/인터페이스 명세 초안

## 9.1 노트북 -> 젯슨 (`dms.input.v1`)

```json
{
  "ts": 1712500000.123,
  "seq": 10231,
  "hands_on": false,
  "eyes_on": false,
  "driver_present": true,
  "pose_confidence": 0.91,
  "vlm": {
    "called": true,
    "reason": "phone",
    "confidence": 0.68,
    "parse_ok": true,
    "latency_ms": 912
  }
}
```

## 9.2 젯슨 -> 대시보드 (`dcas.state.v1`)

```json
{
  "ts": 1712500000.223,
  "dcas_state": "RESTRICTED",
  "driver_state": "WARNING",
  "warning_level": 2,
  "vehicle": {
    "speed": 2.3,
    "steering_cmd": 0.12,
    "throttle_cmd": 0.04,
    "brake_cmd": 0.15
  },
  "hmi_message": "전방을 주시하고 핸들을 잡아주세요",
  "reason_codes": ["eyes_off_timeout", "vlm_phone"]
}
```

## 9.3 젯슨 <-> MCU (`failsafe.signal.v1`, 후순위)

- `soc_alive` (heartbeat)
- `rmf_request`
- `rmf_active`

---

## 10) 리스크 분석 및 완화 전략

## 10.1 기술 리스크

1. VLM latency
   - 완화: 비동기 호출, rate limit, stale 결과 만료 처리
2. VLM 출력 불안정(JSON 파손)
   - 완화: strict parser + fallback reason=`unclear`
3. 오검출/민감도 문제
   - 완화: 히스테리시스, 시간 누적, confidence gating

## 10.2 시스템 리스크

1. 노트북/네트워크 장애
   - 완화: 입력 타임아웃 시 보수 모드(경고 단계 상승)
2. 젯슨 프로세스 다운
   - 완화: heartbeat 감시 + MCU fail-safe(후순위)

## 10.3 운영/보안 리스크

`last project.md`에 API 키가 평문으로 포함되어 있다.

- 즉시 조치 권고:
  - 키 폐기/재발급
  - `.env` 또는 secret store로 이동
  - 레포 커밋/문서에서 제거

---

## 11) 검증 전략 (팀장 관점 KPI 포함)

## 11.1 테스트 계층

1. 유닛 테스트
   - 상태 전이 함수, 타이머 누적 로직, 정책 테이블 매핑
2. 통합 테스트
   - DMS 입력 스트림 리플레이 -> 상태/명령 출력 검증
3. HIL/데모 테스트
   - 실제 카메라 상황(phone/away/drowsy) 시나리오 반복

## 11.2 KPI 제안

- `T_warn` 내 경고 발동률
- false escalation 비율
- VLM 호출당 평균 latency
- 상태 전이 재현율(동일 입력 -> 동일 전이)
- 데모 시나리오 성공률(연속 N회)

---

## 12) 구현 우선순위 (2개월 현실 플랜)

### Phase 1 (즉시, 1주)

- 정책 상태/전이/타이머 문서 고정
- 입력/출력 JSON 스키마 v1 확정

### Phase 2 (2~3주)

- DCAS-PolicyEngine 최소 실행본 구현
- 더미 입력 + 로그/대시보드 연동

### Phase 3 (3~5주)

- MediaPipe 실제 입력 연동
- VLM 연동 + 파서/타임아웃/fallback

### Phase 4 (5~7주)

- 맥락별 대응 튜닝
- 데모 시나리오 자동 재생/검증 스크립트

### Phase 5 (잔여)

- MCU fail-safe(시간 허용 시)
- 문서/발표/근거 정리

---

## 13) DCAS-PolicyEngine 레포에 바로 권장하는 초기 구조

```text
DCAS-PolicyEngine/
  src/
    dcas_policy_engine/
      __init__.py
      models.py            # 입력/출력 dataclass 또는 pydantic
      enums.py             # DriverState, DcasState, ReasonCode
      thresholds.py        # 타이머/임계값
      policy/
        rules.py           # 핵심 정책 규칙
        mapper.py          # reason -> action 매핑
      engine/
        state_machine.py   # 상태 전이 로직
        evaluator.py       # 단일 tick 평가
      io/
        adapters.py        # ZMQ/ROS2/MQTT 연동 어댑터
      hmi/
        messages.py        # 상태별 안내 문구
  tests/
    test_state_machine.py
    test_policy_mapping.py
    test_timeout_fallback.py
  DOCS/
    research.md
    interface-spec-v1.md
    state-machine-v1.md
```

---

## 14) 핵심 결론

1. 당신의 역할은 “기능 구현자”를 넘어 “정책 책임자”다.
2. 현재 ads-skynet은 통신/뷰어/차량 제어 기반이 이미 좋다.
3. 따라서 성패는 `DCAS-PolicyEngine`의 상태 머신/정책 설계 품질에 달려 있다.
4. VLM은 핵심이지만 주 트리거가 아니라 “맥락 강화 신호”로 배치해야 안정적이다.
5. 데모 성공 조건은 알고리즘 정확도 하나가 아니라, 일관된 단계 전이 + 설명 가능한 HMI + 재현 가능한 로그다.

## 15) 구현 언어 전략 (Python-first, C++-ready)

- 단기(시연/검증): Python 우선 구현이 현실적이다.
   - 근거: 현재 ads-skynet 통합 환경(`common`, `lkas`, `Web-viewer`)과 개발 속도
- 중기(산업 고도화): 성능/실시간 요구가 확인되는 경로만 C++로 단계적 포팅한다.
   - 원칙: 메시지 스키마/인터페이스는 유지하고 내부 구현만 교체
- 면접/산업 관점 설명 포인트:
   - “언어 선택은 목적 함수(실시간성, 안전성, 검증 비용, 통합 속도)에 따른 공학적 결정”
   - “Python으로 정책/검증을 빠르게 고정하고, 필요 병목만 C++로 이전하는 전략을 채택”

---

## 외부 참고 링크 (조사 중 접근 가능 자료)

- MediaPipe Pose Landmarker: https://ai.google.dev/edge/mediapipe/solutions/vision/pose_landmarker
- ROS2 QoS (Humble): https://docs.ros.org/en/humble/Concepts/Intermediate/About-Quality-of-Service-Settings.html
- Open Policy Agent (OPA): https://www.openpolicyagent.org/docs/latest/
- NHTSA Automated Vehicles for Safety: https://www.nhtsa.gov/vehicle-safety/automated-vehicles-safety

참고: UNECE 원문 페이지는 봇 차단/접근 제한으로 자동 수집이 불안정했다. 다만 `last project.md`에 이미 정리된 R171 핵심 항목(engagement, warning escalation, non-response 절차)은 본 문서의 정책 요구사항으로 반영했다.
