# Control Policy Baseline (Step C)

본 문서는 `DCAS-PolicyEngine`의 Step C(정책 결정/제어 제한) **기준선(Baseline)** 명세다.
Step B 상태(`OK -> WARNING -> UNRESPONSIVE -> ABSENT`)를 입력받아,
제어 제한·HMI·MRM 활성 여부를 일관되게 산출한다.

핵심 목표:

1. 규정 앵커(UNECE/Euro NCAP)와 충돌하지 않는 대응 정책 고정
2. 규정 고정값(A)과 캘리브레이션 항목(B/C) 분리

---

## 1) Step C 목적과 범위

- 목적:
  - 운전자 상태 악화 시 위험 완화(감속/경고 강화)
  - 재참여 유도(EOR/DCA 메시지 일관화)
  - Driver Unavailability 단계에서 최소위험 정지 방향으로 전환
- 범위:
  - `throttle_limit`: 상태별 제어값
  - `hmi_action`: 경고/안내 메시지 (INFO/EOR/DCA/MRM)
  - `mrm_active`: ABSENT 진입 시 MRM 감속 모드 활성 여부
- 비범위:
  - 상태 전이 타이밍 계산(= Step B)
  - 저수준 제어기(PID/MPC) 내부 파라미터 튜닝

---

## 2) 입력/출력 인터페이스

### 2.1 입력

- `driver_state`: `OK | WARNING | UNRESPONSIVE | ABSENT`
- `reason`: `phone | drowsy | unresponsive | intoxicated | none | unknown`
- `lkas_throttle`: LKAS가 요청한 원시 종방향 제어값
- `input_stale`: 센서/파싱 stale (**프로토타입 v0에서는 미사용, 예약 필드**)
- `aeb_active` (optional): 긴급 제동/충돌회피 시스템 활성 상태
- `driver_override` (optional): 운전자가 비지원 제어(조향/제동/가속)로 직접 인수했는지 여부
- `lkas_mode` (optional): `OFF | ON_INACTIVE | ON_PASSIVE | ON_ACTIVE`
- `notebook_input_alive` (optional): 노트북(인지 입력) 수신 유효 여부
- `manoeuvre_state` (optional): `NONE | PRE_ANNOUNCED | IN_PROGRESS`
- `manoeuvre_type` (optional): `NONE | CURVE_FOLLOW | LANE_CHANGE | TURN | MRM`
- `next_manoeuvre_type` (optional): `NONE | CURVE_FOLLOW | LANE_CHANGE | TURN | MRM`
- `next_manoeuvre_eta_s` (optional): 다음 기동 시작까지 남은 시간(초)
- `manoeuvre_notice_lead_s` (optional): 시스템 주도 기동 사전 통지 리드타임(초)
- `reason_context_source` (optional): `perception_notebook | step_b_bridge` (맥락 입력 출처 추적용)

### 2.1.1 맥락 입력 소비 규칙 (용어 통일)

- 인지팀은 inattentive 판단 시 노트북에서 VLM을 직접 호출하고 맥락(`reason`)을 전달한다.
- Step C는 `reason`을 직접 소비하며, 별도 VLM 호출 요청 신호는 사용하지 않는다.
- 정책 결정 우선순위는 항상 `driver_state`가 최상위이고, `reason`은 overlay/HMI 보강 신호다.

설계 원칙:

- 속도/곡률 맥락은 Step B 전이 임계값에서 이미 반영되므로,
  Step C는 **상태 기반 강도 정책**으로 단순화한다(이중 반영 방지).
- 인터페이스 호환을 위해 입력 `manoeuvre_type`은 출력 `dashboard_state.current_manoeuvre_type`으로 매핑한다.

### 2.2 출력

- `throttle_limit`: 상태별 제어값 제한
- `hmi_action`: 경고/지시 메시지 (상태에 따라 결정)
- `mrm_active`: `true | false` (ABSENT 진입 시 `true`)
- `dashboard_state` (권장):
  - `lkas_mode`: `OFF | ON_INACTIVE | ON_PASSIVE | ON_ACTIVE`
  - `current_manoeuvre_type`: `NONE | CURVE_FOLLOW | LANE_CHANGE | TURN | MRM`
  - `next_manoeuvre_type`: `NONE | CURVE_FOLLOW | LANE_CHANGE | TURN | MRM`
  - `next_manoeuvre_eta_s`: 다음 기동 시작 예정까지 남은 시간(초)
  - `hmi_action`: INFO/EOR/DCA/MRM 경고/지시 메시지
  - `reason_context_source`: 맥락 입력 출처 추적 정보(옵션)

추가 해석(R171규정 정합):

- `lkas_mode`는 아래 순서로 진행한다.
  - `OFF`: 사용자가 ON 요구를 하지 않은 상태
  - `ON_INACTIVE`: 사용자가 ON을 눌렀지만 LKAS 시작 조건이 아직 충족되지 않은 상태
  - `ON_PASSIVE`: ON은 눌렸고, LKAS 시작 조건이 충족되어 대기 중인 상태
  - `ON_ACTIVE`: LKAS가 실제로 제어를 수행하는 상태

LKAS 시작 조건(Stand-by/Inactive -> Stand-by/Passive 진입 조건):

- `driver_state == OK`
- `notebook_input_alive == true` (노트북 입력 수신 가능)

운전자 최종 승인에 의한 활성화(Stand-by/Passive -> Active):

- 운전자가 `SET` 버튼/속도 세팅 등 최종 승인 조작을 수행하면 `ON_ACTIVE`로 **즉시 전이**한다.
- 이 전이는 3초 대기 없이 즉시 제어 개입을 시작한다.

시스템 주도 기동 리드타임(활성화와 분리):

- 최소 3초 사전 통지(`manoeuvre_notice_lead_s >= 3.0s`) 규칙은 **시스템 주도 기동**에만 적용한다.
- 즉, "운전자가 시스템을 활성화하는 행위"와 "시스템이 주도적으로 방향을 크게 바꾸는 기동"은 분리해서 다룬다.

전이 규칙(모드 계층):

- `OFF -> ON_INACTIVE`: 사용자가 ON 버튼/조작을 수행했으나 시작 조건이 아직 미충족
- `ON_INACTIVE -> ON_PASSIVE`: 시작 조건 충족 시 대기 상태로 진입
- `ON_PASSIVE -> ON_ACTIVE`: 운전자 최종 승인(`SET`/속도 세팅) 직후 즉시 제어 개입 시작
- `ON_ACTIVE -> ON_PASSIVE`: 제어 조건 상실/일시 중단 시
- `ON_PASSIVE -> ON_INACTIVE`: ON은 유지되나 시작 조건을 다시 잃은 경우
- `ON_INACTIVE/ON_PASSIVE/ON_ACTIVE -> OFF`: 사용자가 OFF를 누르거나 시스템이 종료될 때

대시보드 의미 규칙:

- `manoeuvre_state=PRE_ANNOUNCED`는 "곧 수행할 기동" 예고 상태다.
- `manoeuvre_state=IN_PROGRESS`는 "현재 수행 중인 기동" 상태다.
- 현재 기동의 종류(커브 추종/MRM 등)는 `current_manoeuvre_type`으로 표시한다.
- 다음 예정 기동은 `next_manoeuvre_type`과 `next_manoeuvre_eta_s`로 함께 표시한다.
- `manoeuvre_state=NONE`이면 `next_manoeuvre_type=NONE`, `next_manoeuvre_eta_s=null`을 권장한다.
- 운전자 개입 요구(EOR/DCA/Unavailability)는 별도 enum이 아니라 `hmi_action`으로 전달한다.
- `lkas_mode`는 “전원 상태”가 아니라 “사용자 요청/시작 조건/제어 개입 수준”을 함께 표현하는 상태다.

---

## 3) 규정/평가 앵커 (Step C 고정 제약)

| 앵커 | Step C에 필요한 해석 | 출처 |
|---|---|---|
| 시스템 제어는 충돌 위험을 줄이되 운전자 개입 가능해야 함 | 급격/과도한 제어를 피하고, 항상 운전자 개입 가능 상태 유지 | UNECE R171 5.3.4, 5.3.6.1[^c1] |
| 경계/종료 시 보조 기능은 controllable way로 종료 | `UNRESPONSIVE/ABSENT`에서도 종료·감속 전략은 조향 안정성 유지 | UNECE R171 5.3.5.2.1[^c1] |
| EOR는 시각 + 타 모달리티, DCA는 즉시 수동 인수 명령 | WARNING/UNRESPONSIVE의 HMI는 다중모달 구조로 고정 | UNECE R171 5.5.4.2.3.2~3[^c1] |
| 경고 에스컬레이션 시 emergency 시스템 경고 우선 고려 | AEBS 등과 동시 활성 시 HMI arbitration 필요 | UNECE R171 5.5.4.2.2.2[^c1] |
| 운전자가 모드/기동 상황을 오인하지 않도록 명확한 HMI 필요 | 현재 수행 기동 + 다음 예정 기동을 구분 표기하여 모드 혼동 최소화 | UNECE R171 5.5.1, 5.5.2, 5.5.4.1[^c1] |

해석 원칙:

- 규정은 "정확한 throttle/steer 비율"을 직접 강제하지 않는다.
- 따라서 비율값은 캘리브레이션 대상(B/C)이지만,
  - 에스컬레이션 구조,
  - 다중모달 경고,
  - controllability,
  - emergency 우선순위
  는 고정(A)으로 취급한다.

---

## 4) Core Action Mapping (기준선 1차값)

> 아래 수치는 JetRacer급 플랫폼의 보수 기준선이다. 차량 플랫폼별 재튜닝 전제로 사용한다.

| DriverState | throttle_limit | HMI | mrm_active |
|---|---:|---|---|
| OK | `1.00 * lkas_throttle` | 정보성 표시만 | false |
| WARNING | `<= 0.60 * lkas_throttle` | EOR (반복 음향/햅틱) | false |
| UNRESPONSIVE | `<= 0.20 * lkas_throttle` | DCA (즉시 수동 인수) | false |
| ABSENT | `0.0` | MRM + 최대 경고 | **true** |

실행 규칙:

- 상위 상태 완화 금지: `ABSENT > UNRESPONSIVE > WARNING > OK`
- ABSENT 상태에서 `throttle_limit = 0.0` 및 `mrm_active = true`
  - `throttle_limit = 0.0`은 **속도 0**이 아니라 **추가 종방향 출력 금지**를 뜻한다
  - `mrm_active = true`일 때 MRM 감속 모드(능동 제동) 즉시 활성화
  - 권장 감속도: `a_mrm_cmd <= -2.0 m/s^2` (플랫폼/브레이크 HW capability에 맞춰 캘리브레이션)
- 상태 전이 시 `throttle_limit` 목표가 급변하면, 최종 인가값은 반드시 Rate Limiter 또는 LPF를 통과
  - 권장 예: `|d(throttle_limit_cmd)/dt| <= 0.1 /s`

---

## 5) Reason Overlay (상태 기반 정책 위에 덧씌움)

| reason | 적용 상태 | 제어값 영향 | HMI 문구/동작 |
|---|---|---|---|
| `phone` | WARNING+ | 제어값 추가 보수화 없음(Core 유지) | "전방주시 필요(휴대폰 감지)" + 비프 주기 단축 |
| `drowsy` | WARNING+ | `throttle_limit` 추가 10% 보수화 | "졸음 경고, 즉시 주시" + 휴식 유도 문구 |
| `unresponsive` | UNRESPONSIVE/ABSENT | Core 완화 금지 | "반응 없음, 감속/정지 유도" |
| `intoxicated` | WARNING+ | `throttle_limit` 추가 20% 보수화 | "음주 의심, 즉시 안전 인수" + 강한 반복 경고 |
| `none/unknown` | 모든 상태 | Core만 적용 | 일반 eyes-on 경고 |

Overlay 불변식:

- Overlay는 상향 보수화만 허용(완화 금지)
- `input_stale` 기반 우회 로직은 프로토타입 v0에서 적용하지 않는다(추후 fail-safe 단계에서 활성화)

---

## 6) HMI 에스컬레이션 시퀀스 (Step B 타이머와 결합)

이 섹션은 **상태 전이 사실을 운전자에게 알리고**, 단계별로 필요한 행동(주시 복귀/즉시 인수)을
명령형으로 전달하는 HMI 규칙이다.

| 단계 | Step B 상태 기준 | Step C HMI 요구 |
|---|---|---|
| Stage 0 | `OK` | 정보성 표시만 |
| Stage 1 | `WARNING` 초기 | EOR: 연속 시각 + 음향/햅틱 최소 1개 |
| Stage 2 | `WARNING` 지속 | 에스컬레이션: 강도 증가(반복/증폭) |
| Stage 3 | `UNRESPONSIVE` | **DCA: "즉시 수동 인수" 명령형 경고(즉시 표시)** |
| Stage 4 | `ABSENT` | MRM (Minimum Risk Maneuver) + 최대 경고 |

### 6.1 `hmi_action` 표시 내용 표준 (필수)

`hmi_action`은 아래 4개 레벨로 고정한다.

- `INFO`
- `EOR`
- `DCA`
- `MRM`

| `hmi_action` | 트리거 조건 | 표시 텍스트(기본) | 표시/알림 강도 | 해제 조건 |
|---|---|---|---|---|
| `INFO` | `driver_state=OK` 일반 주행 | "시스템 정상 대기/주행" | 정보성 시각 표시만 | 상태 악화 시 상위 레벨로 즉시 승격 |
| `INFO` | `driver_override=true` 이후 재개 대기 | **"자동 재개 없음, 운전자 의도 조작 필요"** | 정보성 시각 표시 + 고정 배너 권장 | 운전자 명시 조작으로 재활성화 완료 시 |
| `EOR` | `driver_state=WARNING` (초기/지속) | "전방 주시 필요" / "핸들을 잡아주세요" | 연속 시각 + 음향/햅틱(최소 1개), 지속 시 반복/증폭 | `recover_elapsed>=200ms` 시 경고 채널 완화 가능(상태 유지), Step B 복귀(`OK`) 시 완전 해제 |
| `DCA` | `driver_state=UNRESPONSIVE` | **"즉시 수동 인수"** | 명령형 최대 가시성 + 강한 반복 음향/햅틱 | 운전자 인수 확인(`driver_override=true`) 또는 `MRM` 승격 |
| `MRM` | `driver_state=ABSENT` 또는 `mrm_active=true` | **"운전자 부재 - 안전 정지 중"** | 최대 경고(시각/음향/햅틱) + 진행 상태 고정 표시 | run cycle 정책상 자동 해제 금지(수동/재시동 정책 따름) |

문구 구성 규칙:

- `reason`이 있으면 부가 문구를 suffix로 추가한다. 예: `EOR + drowsy -> "전방 주시 필요 (졸음 경고)"`
- 동일 시점 다중 요청 충돌 시 우선순위는 `MRM > DCA > EOR > INFO`를 적용한다.

정합 조건:

- Stage 전환 시점은 Step B 규정 앵커(5s/3s/5s/10s)와 반드시 동기화한다.
- 경고 채널 충돌 시 emergency 시스템 경고를 우선 표시한다.

---

## 7) 충돌 해소(Arbitration) 및 Fail-safe

### 7.1 다중 시스템 우선순위 (향후 AEBS 추가 시)

**현재 (LKAS만):** 해당 사항 없음

**향후 AEBS 추가 시:**
- `aeb_active=true`면:
  - Step C HMI를 보조 채널로 격하(메인 경고는 AEBS)
  - 종방향(`throttle_limit`)은 더 보수적인 값 선택(`min` 규칙)

### 7.2 stale/fault 처리 (프로토타입 v0)

- 프로토타입 v0에서는 `input_stale`를 정책 분기 조건으로 사용하지 않는다.
- stale/fault fail-safe는 v1 안전강화 단계에서 활성화한다.

### 7.3 경계/종료 controllability

- 시스템 종료/경계 초과/오류 상황에서도 급격한 종방향 변화 금지
- 조향은 LKAS 원출력을 유지하고, DCAS는 종방향 제한 및 HMI/긴급 상태만 중재

### 7.4 LKAS 시작 전제조건/기동 통지 fail-safe

- `driver_state != OK` 또는 `notebook_input_alive=false`면:
  - `lkas_mode`를 `ON_ACTIVE`로 승격하지 않는다(`ON_INACTIVE` 또는 `OFF` 유지).
  - 운전자 개입 요구는 `hmi_action`을 통해 최소 `EOR` 이상으로 유지한다.
- 시스템 주도 기동인데 `manoeuvre_notice_lead_s < 3.0s`이면:
  - `manoeuvre_state=PRE_ANNOUNCED/IN_PROGRESS` 전환을 금지하고,
  - `next_manoeuvre_type=NONE`, `next_manoeuvre_eta_s=null`로 강등하여 오인 가능성을 차단하고,
  - 대시보드에 "기동 사전 통지 부족" 경고를 출력한다.

### 7.5 DCA 인수/운전자 부재 응답 이후 재활성화 규칙

- 시나리오 A: DCA 직후 운전자가 직접 인수(`driver_override=true`)
  - DCA는 즉시 해제한다(요청 확인 완료).
  - 시스템은 `ON_ACTIVE`를 유지하지 않고 `ON_PASSIVE`, `ON_INACTIVE` 또는 `OFF`로 전환한다.
  - 주행 보조 재개는 반드시 운전자 의도 조작으로만 허용한다(자동 재개 금지).

- 시나리오 B: DCA 미응답으로 ABSENT 진입 (MRM 활성화)
  - 해당 이벤트를 `mrm_activation_count_run_cycle`로 누적 관리한다.
  - 같은 run cycle에서 반복 MRM이 누적되면 활성화 잠금(lockout)을 적용한다.
  - 기본 권고: `mrm_activation_count_run_cycle > 1`이면 run cycle 종료 전까지 DCAS 재활성화 금지.
  - lockout 해제는 run cycle 재시작(차량 재시동) 이후에만 허용한다.

주의(규정 해석):

- R171 5.5.4.2.8.1은 반복/장기 이탈에 대한 활성화 차단 전략을 요구하며,
  최소 기준은 "같은 run cycle에서 MRM이 1회를 초과"하는 경우다.
- 즉, 단 1회 MRM만으로 무조건 lockout은 규정 고정값이라기보다 제조사 보수 정책 영역이다.

대시보드 송신 필드:

- `lkas_mode` (`OFF/ON_INACTIVE/ON_PASSIVE/ON_ACTIVE`)
- `current_manoeuvre_type` (`NONE/CURVE_FOLLOW/LANE_CHANGE/TURN/MRM`)
- `next_manoeuvre_type` (`NONE/CURVE_FOLLOW/LANE_CHANGE/TURN/MRM`)
- `next_manoeuvre_eta_s` (초, 없으면 `null`)
- `hmi_action` (상태별 경고/안내: INFO/EOR/DCA/MRM)
- `mrm_active` (`true/false`): ABSENT 상태일 때 `true`

---

## 8) 캘리브레이션 항목 (B/C) vs 고정 앵커 (A)

### 8.1 근거 강도 등급

- `A`: 규정 본문 또는 공식 시험 요건에 직접 명시
- `B`: 공식 프로토콜 + 연구 경향으로 방향성이 강함
- `C`: 플랫폼 제약 기반 운영 가정

### 8.2 분류표

| 항목 | 분류 | 근거 강도 | 비고 |
|---|---|---|---|
| 다중모달 EOR/DCA 요구 | 확정값 | A | UNECE R171 5.5.4.2.3.2~3[^c1] |
| controllable termination 보장 | 확정값 | A | UNECE R171 5.3.5.2.1[^c1] |
| 상태별 throttle 비율값 | 가정값 | C | JetRacer baseline, 플랫폼별 재튜닝 |
| reason overlay 보수화 | 가정값 | C | 운영 안정성 가정 |
| 상태 전이 시 Rate Limiter/LPF | 가정값(권장) | B | UNECE controllability 취지 정합[^c1] |
| ABSENT → MRM 즉시 전환 | 확정값 | A | ABSENT 상태 진입 시 `mrm_active=true` (R171 5.3.7.3) |

---

## 9) 구현 인터페이스

```python
def evaluate_policy(
    driver_state: DriverState,
    reason: Reason,
    lkas_throttle: float,
    lkas_mode: str = "OFF",
    current_manoeuvre_type: str = "NONE",
    next_manoeuvre_type: str = "NONE",
    next_manoeuvre_eta_s: float | None = None,
  reason_context_source: str = "step_b_bridge"
) -> PolicyOutput:
    """
    상태에 따라 제어값, HMI, MRM 활성 여부 결정
    """
    # 상태별 기본값
    policy_map = {
      DriverState.OK: (1.00, "INFO", False),
      DriverState.WARNING: (0.60, "EOR", False),
        DriverState.UNRESPONSIVE: (0.20, "DCA", False),
        DriverState.ABSENT: (0.0, "MRM", True),
    }
    
    ratio, hmi, mrm_active = policy_map.get(driver_state, (1.0, "INFO", False))

    # reason overlay (prototype v0)
    reason_overlay = {
        "phone": 1.00,
        "drowsy": 0.90,
        "unresponsive": 1.00,
        "intoxicated": 0.80,
        "unknown": 1.00,
        "none": 1.00,
    }
    overlay_gain = reason_overlay.get(reason, 1.00)

    throttle_limit = ratio * overlay_gain * lkas_throttle

    dashboard_state = {
        "lkas_mode": lkas_mode,
        "current_manoeuvre_type": current_manoeuvre_type,
        "next_manoeuvre_type": next_manoeuvre_type,
        "next_manoeuvre_eta_s": next_manoeuvre_eta_s,
        "hmi_action": hmi,
      "reason_context_source": reason_context_source,
    }
    
    return PolicyOutput(
        throttle_limit=throttle_limit,
        hmi_action=hmi,
        mrm_active=mrm_active,
        dashboard_state=dashboard_state
    )
```

권장 단위 테스트:

- **상태 단조성**: 상위 상태(`ABSENT`)에서 `throttle_limit`이 하위 상태보다 작거나 같음
- **MRM 활성**: `driver_state=ABSENT`일 때만 `mrm_active=true`
- **reason 처리**: `reason=intoxicated`에서 Overlay 보수화 규칙이 적용되는지 확인
- **비율 적용**: 각 상태별 비율값이 정확히 적용되는지 확인

---

## 10) 참고문헌

[^c1]: UNECE, *UN Regulation No. 171 (E/ECE/TRANS/505/Rev.3/Add.170)*, `DOCS/R171e.pdf` (5.3.4, 5.3.5.2.1, 5.3.6.1, 5.5.4.2.2.2, 5.5.4.2.3.2~3).
[^c2]: Euro NCAP, *Safe Driving Driver Engagement Protocol v1.1* (2025), section 1.4.1~1.4.3.3. https://cdn.euroncap.com/cars/assets/euro_ncap_protocol_safe_driving_driver_engagement_v11_a30e874152.pdf
[^c3]: Peng et al., *Driver’s lane keeping ability with eyes off road: Insights from a naturalistic study*, Accid Anal Prev, PMID: 22836114. https://pubmed.ncbi.nlm.nih.gov/22836114/
[^c4]: Liang et al., *How dangerous is looking away from the road?*, Human Factors, PMID: 23397818. https://pubmed.ncbi.nlm.nih.gov/23397818/
