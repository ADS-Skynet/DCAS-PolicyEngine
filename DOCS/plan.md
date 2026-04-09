# DCAS-PolicyEngine 상세 구현 계획 (2026-04-08)

본 문서는 `research.md`의 정책/아키텍처 리서치를 바탕으로 실제 코드베이스에 적용할 상세 구현 계획을 제시한다. 구현은 하지 않고, 설계/접근법/파일경로/코드스니펫/트레이드오프를 명확히 기술한다.

---

## 1. 접근 방식 (Overview)

- OPA(Policy-as-Code) 원칙에 따라 정책 로직과 실행 코드를 분리
- 입력/출력 스키마(pydantic/dataclass)로 계약 고정, 버전 관리
- 상태머신(State Machine) 기반 정책 전이 구현
- 테스트/시나리오/문서화 병행
- 인지팀과의 인터페이스(입력 스키마) 동기화

### 1.1 팀 역할 분담 원칙 (확정)

- 인지팀: 인지 신뢰도/품질 강화에 집중 (물리/인지 신호 산출)
- 정책팀(본 레포): 판단 역할 확보 (상태 전이 + 정책 결정 + 제어 지시)
- HMI팀: 프론트엔드 디자인/표현 계층 담당

경계 원칙:

- 인지팀은 `driver_state`를 최종 확정하지 않고, 원시/정규화 신호를 전달
- 정책팀이 `driver_state`/`dcas_state`를 authoritative source로 산출

---

## 2. 주요 파일/디렉토리 구조 및 역할

- `src/dcas_policy_engine/models.py` : 입력/출력 데이터 모델 정의 (pydantic/dataclass)
- `src/dcas_policy_engine/enums.py` : DriverState, DcasState, ReasonCode 등 열거형
- `src/dcas_policy_engine/thresholds.py` : 타이머/임계값 상수
- `src/dcas_policy_engine/policy/rules.py` : 정책 규칙 테이블/매핑
- `src/dcas_policy_engine/policy/mapper.py` : reason/action 매핑 함수
- `src/dcas_policy_engine/engine/state_machine.py` : 상태 전이 계산
- `src/dcas_policy_engine/engine/evaluator.py` : 단일 tick 평가/정책 결정
- `src/dcas_policy_engine/io/adapters.py` : ZMQ/ROS2/MQTT 연동 어댑터
- `src/dcas_policy_engine/hmi/messages.py` : 상태별 안내 메시지
- `tests/` : 유닛/통합 테스트
- `DOCS/interface-spec-v1.md` : 입력/출력 JSON 스키마 명세
- `DOCS/state-machine-v1.md` : 상태 전이 표/설명

---

## 3. 상세 구현 단계별 계획

### 3.1 입력/출력 스키마 정의
- pydantic/dataclass로 입력(`dms.input.v1`), 출력(`dcas.state.v1`) 모델 정의
- 예시 JSON, 필드별 타입/설명 주석 포함
- 버전 필드 명시
- **파일:** `src/dcas_policy_engine/models.py`

입력 계약 원칙:

- 입력은 물리/인지 근거 중심(`hands_on`, `eyes_on`, `driver_present`, `pose_confidence`, `vlm.reason`, `vlm.confidence`)
- 최종 상태(`driver_state`)는 정책팀 계산값을 기준으로 사용

#### 코드 스니펫 예시
```python
from pydantic import BaseModel
from typing import Optional

class VLMInput(BaseModel):
    called: bool
    reason: str
    confidence: float
    parse_ok: bool
    latency_ms: Optional[int]

class DMSInput(BaseModel):
    ts: float
    seq: int
    hands_on: bool
    eyes_on: bool
    driver_present: bool
    pose_confidence: float
    vlm: VLMInput
```

### 3.2 상태/정책 전이 로직(상태머신) 설계
- DriverState, DcasState enum 정의
- 상태 전이 규칙/표를 코드로 구현
- 타이머/히스테리시스/신뢰도 로직 포함
- **파일:** `src/dcas_policy_engine/enums.py`, `src/dcas_policy_engine/engine/state_machine.py`, `src/dcas_policy_engine/thresholds.py`

#### 코드 스니펫 예시
```python
from enum import Enum
class DriverState(Enum):
    OK = 'OK'
    WARNING = 'WARNING'
    UNRESPONSIVE = 'UNRESPONSIVE'
    ABSENT = 'ABSENT'
```

### 3.3 정책 결정/매핑
- reason/action 매핑 테이블/함수 구현
- 정책 규칙(경고, 제한, 감속 등) 함수화
- **파일:** `src/dcas_policy_engine/policy/rules.py`, `src/dcas_policy_engine/policy/mapper.py`

#### 코드 스니펫 예시
```python
REASON_POLICY = {
    'phone': {'risk': 'High', 'actions': ['BEEP', 'BRAKE_PULSE']},
    'drowsy': {'risk': 'High', 'actions': ['LONG_BEEP', 'SLOWDOWN']},
    # ...
}
```

### 3.4 평가/출력 생성
- 단일 tick 평가 함수(evaluator) 구현
- 입력→상태→정책→출력 JSON 생성
- **파일:** `src/dcas_policy_engine/engine/evaluator.py`

### 3.5 통신 어댑터/연동
- ZMQ/ROS2/MQTT 등 메시지 송수신 어댑터 설계
- **파일:** `src/dcas_policy_engine/io/adapters.py`

### 3.6 테스트/시나리오
- 상태 전이/정책 매핑/예외 처리 유닛 테스트
- 입력 스트림 리플레이 통합 테스트
- **파일:** `tests/test_state_machine.py`, `tests/test_policy_mapping.py`, `tests/test_timeout_fallback.py`

### 3.7 문서화
- 입력/출력 스키마, 상태 전이 표, 정책 매핑 문서화
- **파일:** `DOCS/interface-spec-v1.md`, `DOCS/state-machine-v1.md`

---

## 4. 트레이드오프 및 고려사항

- **pydantic vs dataclass**: 타입 검증/문서화 용이성(pydantic) vs 경량/속도(dataclass)
- **상태머신 구현**: 단순 if/else vs 테이블 기반 전이(확장성/가독성)
- **정책 매핑**: 하드코딩 vs 외부 정책 파일(유연성/테스트 용이성)
- **통신**: 단일 프로토콜(ZMQ) vs 다중 프로토콜(ROS2/MQTT 병행)
- **테스트**: 실제 입력 리플레이 vs Mock 기반 단위 테스트

### 4.1 구현 언어 선택 (면접/산업 설명 포함)

- **Python-first (현재 권장)**
    - 장점: 기존 생태계와 통합 용이, 시연 일정 대응, 정책 실험/튜닝 속도
    - 단점: 극한 실시간 경로에서 성능 한계 가능
- **C++-first (조건부)**
    - 장점: 실시간/성능/자원 제약 대응에 유리
    - 단점: 초기 통합/개발 속도 저하, 팀 공통 유틸 재구축 비용
- **권장 전략**: Python으로 정책/인터페이스 고정 후, 병목 경로만 C++로 단계적 포팅
    - 면접 답변 포인트: “언어 선호가 아닌 요구사항 기반 단계적 최적화 전략 채택”

---

## 5. 향후 확장/유지보수 포인트

- 정책/상태/스키마 버전 관리 체계
- 정책 테이블 외부화(예: YAML/JSON)
- MCU 연동/Fail-safe 확장
- 대시보드/HMI 메시지 다국어 지원
- 정책/상태 전이 로깅 및 리플레이 도구

---

> 본 계획은 실제 구현 전 검토/피드백을 위한 초안입니다. 추가 요구/변경사항은 별도 지시 바랍니다.
