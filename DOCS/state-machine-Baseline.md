# State Machine v1 (Step B Baseline)

본 문서는 `DCAS-PolicyEngine`의 Step B(운전자 상태 전이) **기준선(Baseline)** 명세다.
핵심 목표는 다음 두 가지다.

1. 규정/연구 기반으로 재현 가능한 전이 로직 정의
2. 기준선(고정 앵커)과 튜닝 대상(캘리브레이션 항목) 명확 분리

제어 제한/출력 정책(Step C)은 본 문서 범위에 포함하지 않는다.
단, `driver_override`에 따른 시스템 OFF 전환은 Step C가 담당한다.

---

## 1) Step B 목적과 범위

- 목적: 운전자 참여 상태를 `OK -> WARNING -> ESCALATION -> ABSENT`로 전이
- 범위: 상태 계산(타이머, 임계값, 히스테리시스, fail-safe)
- 비범위: throttle/steer 제한, HMI 상세 정책, ABSENT 진입 시 MRM 활성화 (= Step C 문서)

---

## 2) 입력/출력 인터페이스

### 2.1 입력

- 필수 입력
  - `is_attentive` (`yes/no`)  
    - 인지팀에서 실시간으로 전송하는 운전자 집중 여부
  - `is_attentive_ts_ms`
    - 인지팀이 `is_attentive` 판단에 부여한 시각(ms)
  - `reason` (`phone | drowsy | unresponsive | intoxicated | none | unknown`)
    - 인지팀에서 집중 안 한 원인을 단일 값으로 즉시 전송
    - 매 주기 현재 `is_attentive` 판단에 대응하는 `reason` 1개만 유효하다
  - `reason_ts_ms`
    - 인지팀이 `reason` 판단에 부여한 시각(ms)
  - `jetracer_input_0_4`
    - JetRacer 런타임 입력값(범위 `0.0 ~ 0.4`)
    - 물리적 `km/h`가 아니라 주행 맥락용 정규화 입력이다
  - `current_state` (현재 운전자 상태)
- 선택 입력
  - `inattentive_elapsed`  
    - 외부에서 주는 값이 아니라 Step B 내부 누적 타이머(기본)
    - inattentive 시작 시점부터의 **누적 절대시간**으로 해석한다
  - `input_stale` (센서 stale/파싱 실패 플래그)
    - 프로토타입 v0에서는 사용하지 않으며, 런타임에서 항상 `false`로 취급
  - `road_curvature`
    - LKAS가 제공하는 현재 주행 커브 맥락(정책 전이 미사용, 상태 표시/로그 용도)

내부 파생값(입력 아님):

- `warning_elapsed`는 WARNING 진입 시각으로부터 Step B 내부에서 계산하는 진단값이다.
- `absent_latched_run_cycle`는 ABSENT 도달 여부를 보존하는 내부 래치이며, 같은 run cycle에서는 해제하지 않는다.
  - 저장 주체는 Step C와 동일한 `DCAS-PolicyEngine runtime/state-store`를 사용한다.

### 2.2 출력

- `next_state`
- `reason` (Step C 전달용 계산 사용 맥락)
- `input_snapshot_ts_ms` (선택): Step B가 이번 계산에 사용한 최신 `is_attentive` 판단 시각

### 2.3 band 경계값 정의 (스케일카 기준)

> 경계값도 기준선의 일부로 고정하며, 플랫폼별 튜닝 시 버전 태깅하여 변경한다.
> 문서상 이름은 `speed_band`를 유지하지만, 실제 계산 입력은 JetRacer의 `0.0 ~ 0.4` 정규화 입력이다.

- `speed_band`는 정규화 속도로 정의
  - `rho_v = jetracer_input_0_4 / 0.4`
  - `LOW`: `rho_v < 0.30`
  - `MID`: `0.30 <= rho_v < 0.65`
  - `HIGH`: `rho_v >= 0.65`

- 속도 추정 노이즈 대응(필수)
  - 원시 `rho_v_raw`를 그대로 band 판정에 사용하지 않는다.
  - 1차 저역통과필터(LPF) 적용:
    - `rho_v_f[k] = alpha * rho_v_f[k-1] + (1-alpha) * rho_v_raw[k]`
    - 권장 시작값: `alpha = 0.85 ~ 0.95` (주기 `dt` 50~100ms)
  - 히스테리시스 band 경계 적용(예시):
    - `LOW -> MID`: `rho_v_f > 0.32`
    - `MID -> LOW`: `rho_v_f < 0.28`
    - `MID -> HIGH`: `rho_v_f > 0.67`
    - `HIGH -> MID`: `rho_v_f < 0.63`
  - 최소 유지시간(dwell) 적용:
    - 경계 조건이 `T_band_hold` 이상 유지될 때만 band 전환
    - 권장 시작값: `T_band_hold = 0.3s`

- 구현 순서(고정)
  - `rho_v_raw` 계산 -> LPF -> 히스테리시스 + dwell -> `speed_band` 결정
  - 임계시간 계산(`T_warn_eff`, `T_esc_eff`, `T_absent_eff`)은 안정화된 `speed_band`만 사용
  - `steer_band`/`road_curvature`는 모니터링/로그용으로만 사용하고, 전이 임계시간 보정에는 사용하지 않는다.

- `speed_band`의 역할
  - 경고/복귀 기준을 직접 결정하는 값이 아니라, 현재 주행 맥락의 거칠기(level)를 나누는 보조 축이다.
  - 주행 입력이 강하고 조향 부하가 큰 구간은 더 보수적인 임계시간을 쓰기 위한 분류용이다.
  - 실제 판단의 주축은 여전히 `is_attentive`와 그 지속시간(`inattentive_elapsed` / `recover_elapsed`)이다.
  - 커브 구간 추가 보정계수를 두지 않는 이유: LKAS가 커브에서 이미 속도를 낮추므로 위험도는 `speed_band`에 간접 반영된다.


초기 스케일카 권장:

- `V_safe_max_kmh`: 스케일카 직선 안전최대속도(측정값)

주의:

- speed band 경계값(`0.30`, `0.65`)은 캘리브레이션 항목이며, 변경 시 로그 근거를 함께 남긴다.
- `steer_band` 경계는 정책 전이에 영향을 주지 않는 표시용 기준값이다.

---

## 2.4 운전자 맥락 (Reason) 분류

**[신규 요구사항]** 인지팀이 제공하는 `reason` 필드를 Step B에 통합.
이는 **집중 안 한 원인**을 분류하여 즉시 상향 전이/Step C 대응 강도 결정을 위한 의미 신호를 제공한다.

| Reason | 의미 | 특성 | 위험도(상대) | 회복 가능성 |
|---|---|---|---|---|
| `drowsy` | 졸음 운전 (눈 감김/PERCLOS) | 반응시간 ↑↑, 주의력 저하 | 3.0-11.0배 | ✅ 깨어남 신호 필요 |
| `phone` | 핸드폰 사용 (시각 이탈/조작) | 시각 이탈 4.7초, 수동 집중 | 1.3-4.3배 | ✅ 빠른 회복 |
| `unresponsive` | 무반응 징후/경고 미응답 | 경고 반응 저하, 응답 지연 | 8.0-50.0배 | ⚠️ 각성 유도 필요 |
| `intoxicated` | 음주 운전 (반응시간/불안정) | 반응시간 +150ms, 조향 불안정 | 2.0-15.0배 | ⚠️ 긴 회복시간 (30min) |
| `none` | 정상 (집중 있음) | 기본 상태 | 1.0배 | - |
| `unknown` | 불명 | 기본값으로 처리 | 1.0배 | - |

**근거:**
- Drowsy: Simons-Morton et al. (PMCID: PMC3999409), NHTSA LTCCS
- Phone: NHTSA Distraction Guidelines, Feng et al. (2015)
- Unresponsive: 무반응/경고 미응답 상황의 고위험 운용 분류
- Intoxicated: NHTSA DWI Studies, BAC vs 반응시간 연구

**설계 원칙:**
- `reason`은 Step B에서 **즉시 상향 전이(critical)** 및 Step C 전달용 의미 신호로 사용한다.
- `reason`은 Step B 타이머(`T_warn/T_esc/T_absent`)를 보정하지 않는다.
- 즉, 타이머 전이는 `is_attentive` 지속시간과 base 임계값으로만 결정한다.

### 2.4.1 인지 입력 처리 규칙 (고정)

인지팀 입력은 매 주기마다 아래 2개를 함께 수신한다고 가정한다.

- `is_attentive` (`yes/no`)
- `is_attentive_ts_ms`
- `reason` (`phone | drowsy | unresponsive | intoxicated | none | unknown`)
- `reason_ts_ms`

고정 규칙:

1. DCAS의 기본 위험도 전이는 `inattentive_elapsed` 기반으로 계산한다.
2. `critical reason`이 active이면 현재 상태와 무관하게 즉시 `ABSENT`로 상향 전이한다.
   단, 이 규칙은 **`is_attentive` 기준 정규화 이후의 `reason`** 에만 적용한다.
   따라서 `is_attentive=yes`로 정규화된 주기에서는 `reason=none`이므로 critical reason 즉시 상향 규칙이 발동하지 않는다.
3. `non-critical reason`은 즉시 상태 점프를 만들지 않고, `WARNING` 구간에서 지속 업데이트한다.
4. `critical reason`으로 `ABSENT`가 강제되면 Step C에는 반드시 `ABSENT + reason`을 함께 전달한다.
5. Step B의 상태 판단 주축은 항상 최신 `is_attentive`이다.
6. `reason`은 `is_attentive=no`일 때의 부주의 맥락으로만 사용한다.
7. `is_attentive=yes`이면 `reason=none`으로 정규화한다.

`critical reason` 분류(기준선):

- `unresponsive` -> 즉시 `ABSENT`로 상향
- `intoxicated` -> 즉시 `ABSENT`로 상향(운영 정책상 보수)

`non-critical reason` 분류(기준선):

- `phone`, `drowsy`, `unknown`, `none`

### 2.4.2 단일 최신 맥락 입력 규칙

- 인지팀은 매 주기 `reason` 1개만 전송한다.
- 이전 주기의 reason은 유지하지 않으며, 현재 계산 주기의 `is_attentive`에 대응하는 reason만 유효하다.
- 입력된 `reason`이 비정상이거나 누락되면 `unknown`으로 정규화한다.
- `is_attentive=yes`이면 Step B는 `reason=none`으로 강제 정규화한다.
- 상태 전이 판정은 `is_attentive/inattentive_elapsed`가 주축이다.
- 단, `critical reason`이 들어오면 즉시 `ABSENT` 강제 규칙이 타이머 규칙보다 우선한다.
  단, 이는 항상 **현재 계산 주기에서 `is_attentive` 기준으로 정규화된 `reason`** 에 대해서만 적용한다.
- Step C는 Step B가 실제 계산에 사용한 `reason` 1개만 소비한다.

---

## 3) 운전자 맥락별 상태 전이 매트릭스 (신규)

**[Pattern B: 계층 모델]** - 기본 4-state + 맥락 의미 신호

기존 Step B의 4가지 상태는 유지하되, 맥락은 아래 용도로만 사용한다.

- critical 맥락 즉시 `ABSENT` 강제 전이
- Step C/HMI 대응용 계산 사용 reason 전달
- 타이머 전이는 미보정(base 임계값 고정)

### 3.2 복귀 규칙 가드 (핵심)

- 기본 복귀: `is_attentive=yes` 연속 유지 `>= T_recover_hold`이면 `next_state=OK` (단, ABSENT 래치가 없는 경우)
- `unresponsive` 맥락이라도 WARNING/ESCALATION 구간에서는 위 조건을 만족하면 복귀를 허용한다.
- 어떤 이유로든 `ABSENT`에 도달하면 `absent_latched_run_cycle=true`로 고정하고, 같은 run cycle에서는 `OK`로 복귀하지 않는다.

---

## 4) 기본 임계값 (기준선 1차값) - 기존

> 아래 값은 자연주의 연구 경향 + 규정 시한 구조를 반영한 기준선이며,
> 실제 배포값은 플랫폼별 캘리브레이션으로 확정한다.

| speed band | base `T_warn` | base `T_esc` | base `T_absent` | `T_recover_hold` |
|---|---:|---:|---:|---:|
| LOW | 3.0s | 6.0s | 10.0s | 1.0s |
| MID | 2.0s | 4.0s | 8.0s | 1.2s |
| HIGH | 1.5s | 3.0s | 6.0s | 1.5s |

---

## 5) 임계값 적용식

- 프로토타입 v0 기준, Step B는 reason/맥락 기반 타이머 보정을 적용하지 않는다.

적용식:

- `T_warn_eff = T_warn_base(speed_band)`
- `T_esc_eff = T_esc_base(speed_band)`
- `T_absent_eff = T_absent_base(speed_band)`
- `T_recover_hold = T_recover_hold_base(speed_band)` (기본 복귀 유지시간)

해석:

- `T_warn_eff`는 `OK -> WARNING`에만 사용한다.
- `T_esc_eff`, `T_absent_eff`는 모두 inattentive 시작 시점부터의 **누적 절대시간 임계값**이다.
- `WARNING` 이후에는 더 짧은 `T_warn_eff`를 다시 쓰는 것이 아니라, 누적 절대시간 기준 `T_esc_eff`와 `T_absent_eff`를 사용한다.
- 따라서 `WARNING` 상태 자체에 `T_warn`를 다시 깎는 구조는 쓰지 않는다.
- reason은 타이머 크기를 바꾸지 않으며, 즉시 상향 규칙과 Step C 전달 정보로만 사용한다.
- `WARNING` 구간에서는 현재 계산 주기의 `reason`을 매 주기 갱신하고, `WARNING -> ESCALATION` 전이 순간에 실제 사용한 `reason`으로 후속 대응을 결정한다.

안전 하한:

- `T_warn_eff >= 1.0s`

규정 상한(하드 리미트):

- `T_warn_eff <= 5.0s` (UNECE R171 EOR 앵커 보호)
- 구현 권장: `T_warn_eff = clamp(T_warn_eff_raw, 1.0s, 5.0s)`

---

## 6) 전이 규칙 (결정 순서 고정) - 기존 + 맥락 통합

전이 우선순위는 상향(악화) 우선이며, 상태 우선순위는 `ABSENT > ESCALATION > WARNING > OK`이다.

구현 순서(고정, if-else 체인 권장):

1. 현재 계산 주기에서 정규화된 `reason`이 critical reason이면 즉시 `ABSENT`로 상향
2. `inattentive_elapsed` 기반 상향 전이(`OK->WARNING->ESCALATION->ABSENT`)
3. `current_state=ABSENT` 또는 `absent_latched_run_cycle=true`이면 상태를 `ABSENT`로 유지
4. `200ms <= recover_elapsed < T_recover_hold`이면 경고 해제만 허용하고 상태 유지
5. `recover_elapsed >= T_recover_hold`이면 `OK` 복귀(단, ABSENT 래치가 없는 경우)
6. (`v1`에서만) stale fail-safe 적용

| 현재 상태 | 조건 | 다음 상태 | 맥락별 보정 |
|---|---|---|---|
| ANY | 현재 계산 주기의 `reason`이 critical reason | 즉시 `ABSENT` | 타이머 무관 즉시 상향 + 계산 사용 `reason` 동봉 |
| OK | `inattentive_elapsed >= T_warn_eff` | WARNING | reason 타이머 보정 없음 |
| WARNING | `inattentive_elapsed >= T_esc_eff` | ESCALATION | reason 타이머 보정 없음 |
| ESCALATION | `inattentive_elapsed >= T_absent_eff` | ABSENT | reason 타이머 보정 없음 |
| ABSENT | ANY | ABSENT 유지 | `absent_latched_run_cycle=true`, 같은 run cycle 하향 금지 |
| WARNING/ESCALATION | `200ms <= recover_elapsed < T_recover_hold` | 현재 상태 유지 | 재참여 확인(경고 해제 가능), 상태 하향 금지 |
| WARNING/ESCALATION | `is_attentive=yes` 연속 유지 `>= T_recover_hold` | OK | 맥락 종류와 무관하게 복귀 허용 |
| ANY | `input_stale=true` 또는 입력 누락 | stale fail-safe | 맥락 상관없이 최상 유지 |

**복귀 규칙 상세:**

- 기본 복귀: `is_attentive=yes` 연속 유지 `>= T_recover_hold`이면 `next_state=OK` (WARNING/ESCALATION 한정)
- `unresponsive` 맥락에서도 WARNING/ESCALATION 구간에서는 위 조건을 만족하면 복귀를 허용한다.
- 단, `ABSENT` 도달 이력이 있으면 같은 run cycle에서는 `OK` 복귀를 금지한다.

### 6.1 맥락 수신 흐름 (인지팀 주도 VLM)

- 인지팀이 운전자 `is_attentive=no`를 판단하면, 노트북 측에서 VLM을 직접 호출한다.
- 노트북은 VLM 결과를 `reason`으로 Step B에 전달한다.
- Step B는 별도 호출 요청 신호를 보내지 않으며, 최신 `is_attentive`를 기준으로 그 주기에 대응하는 `reason`만 소비한다.
- 상태 전이는 `is_attentive` 타이머 기반으로 독립 동작하고, `reason`은 `is_attentive=no`일 때만 즉시 상향/Step C 전달에 사용한다.
- `is_attentive_ts_ms`, `reason_ts_ms`는 전이 판단의 직접 입력이 아니라,
  인지팀 입력이 현재 논리에 맞게 짝지어 들어오고 있는지 확인하는 진단/로그용 필드로 사용한다.

### 6.1.1 EOR 해제 및 재참여 확인 기준

- EOR는 운전자가 다시 주행 작업 관련 영역으로 시선/머리 자세를 돌린 뒤,
  그 상태가 **최소 200ms 연속 유지**될 때만 해제(confirmed/clear)되는 것으로 본다.
- 따라서 알림을 끄는 조건은 "순간적인 복귀"가 아니라 `recover_elapsed >= 200ms`인 **안정적 재참여**이다.
- `recover_elapsed`가 200ms에 도달하기 전에 다시 시선/머리 자세가 이탈하면,
  누적은 끊기고 EOR는 유지된다.
- 짧은 이탈이 여러 번 반복되면, 제조업체 전략에 따라 다음 중 하나를 적용한다.
  - `T_recover_hold`를 일시적으로 증가시켜 더 긴 안정 구간을 요구한다.
  - 또는 즉시/조기 EOR를 다시 발령하여 반복적인 짧은 복귀를 억제한다.

해석:

- 이 조항은 "알림을 최소 0.2초만 띄우고 끄라"는 뜻이 아니라,
  **재참여가 0.2초 이상 안정적으로 유지되어야 알림을 해제할 수 있다**는 뜻이다.
- 따라서 이 프로젝트에서는 `recover_elapsed`를 EOR 해제와 상태 복귀의 공통 검증 변수로 사용하되,
  경고 해제와 상태 전이는 분리해서 관리한다.
- 구체적으로 `recover_elapsed >= 200ms`에서 경고 해제(재참여 confirmed)는 가능하지만,
  `recover_elapsed < T_recover_hold`인 동안 상태는 하향하지 않는다.

### 6.2 복귀 시 타이머 처리(명시)

복귀 시 `inattentive_elapsed`를 `is_attentive=yes`가 들어왔다고 바로 0으로 만들지 않는다.

권장 타이머 분리:

- `inattentive_elapsed`: `is_attentive=no`일 때만 누적
- `recover_elapsed`: `is_attentive=yes`가 연속 유지되는 시간 누적

처리 규칙:

- `is_attentive=no`가 들어오면 `recover_elapsed = 0`
- `is_attentive=yes`가 들어오면 `recover_elapsed`를 누적하고 `inattentive_elapsed`는 유지
- `current_state == ABSENT` 또는 `absent_latched_run_cycle==true`면 `next_state = ABSENT` 유지
- `200ms <= recover_elapsed < T_recover_hold`이면 경고 해제만 허용하고 `next_state = current_state`를 유지
- `recover_elapsed >= T_recover_hold`가 되면(ABSENT 래치가 없을 때만) `next_state = OK`로 복귀하고 `inattentive_elapsed = 0`, `recover_elapsed = 0`

주의:

- HMI 버튼, 차량 actuation, 기타 비인지 입력은 `is_attentive`를 대체하지 않는다.
- 주의 상태는 오직 인지팀의 `is_attentive` 스트림으로만 판정한다.

의도:

- 짧은 eyes-on 반복으로 타이머를 악용해 경고를 우회하는 패턴(system abuse)을 방지

### 6.3 stale fail-safe 규칙(충돌 방지, v1 안전강화)

프로토타입 v0에서는 `input_stale`를 사용하지 않으며, 아래 규칙은 v1부터 활성화한다.

`input_stale=true` 또는 필수 입력 누락 시 다음 규칙을 적용한다.

- `current_state == OK` 이면 `next_state = WARNING`으로 강제 상향
- `current_state in {WARNING, ESCALATION, ABSENT}` 이면 **현재 상태 동결(freeze)**
- stale 동안 상태 하향(완화) 금지
- stale 해제 후에만 정상 전이/복귀 로직 재개

---

## 7) 임계 시간 산정 방법 (속도 기반, 상세)

질문 포인트:

- "차량 속도에 따라 다음 상태로 넘어가는 inattentive 임계시간이 달라져야 한다"

타당한 방법은 아래 3단계다.

### 7.1 1단계: 기본축 고정 (human-factor 앵커)

- 기본축은 `speed_band`별 테이블을 사용한다.
- 이유:
  - 규정 구조(EOR/escalation/DCA/unavailability)가 시간 축으로 정의됨
  - 자연주의 연구에서 eyes-off 지속시간이 위험 증가와 직접 연관

즉, 기본 `T_warn/T_esc/T_absent`는 사람 반응시간 앵커로 시작한다.

### 7.2 2단계: 프로토타입 고정 적용 (맥락 타이머 보정 미적용)

- 프로토타입 v0에서는 reason/조향 부하에 따른 타이머 추가 보정을 사용하지 않는다.
- `T_warn/T_esc/T_absent`는 `speed_band` 기준 테이블을 그대로 사용한다.
- 맥락(reason)은 즉시 상향 전이와 Step C 전달 신호로만 사용한다.

### 7.3 3단계: 로그 기반 밴드별 보정

임의값 방지를 위해 아래 지표로 조정한다.

- `Late Warning Rate` (위험 구간인데 경고가 늦은 비율)
- `Early Warning Rate` (불필요하게 이른 경고 비율)
- `Recovery Instability` (복귀 직후 재경고 반복)

보정 규칙(권장):

- `Late Warning Rate` 높음 -> 해당 band `T_warn_base` 10% 감소
- `Early Warning Rate` 높음 -> 해당 band `T_warn_base` 10% 증가
- 복귀 흔들림 높음 -> `T_recover_hold` 0.2s 증가

중요:

- 한 번에 큰 폭 조정 금지(한 iteration당 ±10% 이내)
- 각 조정은 최소 3개 시나리오(직선/완만커브/급조향) 로그 후 적용

이 절차가 "임의값" 대신 "근거 기반"으로 시간을 정하는 방법이다.

### 7.4 산업 적용 시 채터링 방지 표준 패턴

실차/양산 ECU에서도 band 경계 채터링은 공통 이슈이며, 보통 아래를 조합한다.

- 신호 안정화: LPF(또는 이동평균)로 추정 속도 노이즈 저감
- 경계 안정화: 히스테리시스 임계값(상향/하향 분리)
- 전이 안정화: 최소 유지시간(dwell), debounce 카운트
- 제어 안정화: band 변경 직후 `T_warn_eff`를 즉시 점프하지 않고 완만 갱신

권장 완만 갱신식:

- `T_warn_eff[k] = beta * T_warn_eff[k-1] + (1-beta) * T_warn_eff_target[k]`
- 권장 시작값: `beta = 0.7 ~ 0.9`

적용 원칙:

- 위험도 상향(더 보수적 band)은 빠르게, 하향(완화)은 느리게 적용
- 노이즈로 인한 단기 왕복보다, 실제 주행 맥락 변화에 반응하도록 시간상 분리

---

## 8) UNECE R171 정합 앵커 (고정)

다음 항목은 Step B 기준선의 규정 앵커다.

- `>10 km/h`에서 시각 disengagement `5s` 지속 시 EOR 필요
- EOR 후 `3s` 이내 DCA(에스컬레이션 포함) 필요
- 첫 DCA 후 `10s` 이내 driver unavailability response 필요
- 재참여 판정 최소 지속시간 `>=200ms`

해석 원칙:

- 위 규정 앵커는 기준선 설계의 참고 자료이며, Step B 구현은 본 문서의 `speed_band`별 시간 테이블을 우선 사용한다.
- 단, `T_warn_eff`의 최대 허용값 `5.0s`는 규정 상한으로 유지한다.

critical 예외 경로(정책 명시):

- 본 기준선은 `unresponsive/intoxicated`를 safety-concept critical 예외로 분류하여 즉시 `ABSENT`로 전이한다.
- 이는 R171 5.5.4.2.6의 "경고 시퀀스는 일부 단계를 건너뛰거나 동시 제공/억제/지연할 수 있음" 허용 범위 내에서, 위험 최소화를 우선한 구현 정책이다.

출처: `DOCS/R171e.pdf` (5.5.4.2.5.2.1, 5.5.4.2.6.2.1, 5.5.4.2.6.2.2, 5.5.4.2.6.3.1, 5.5.4.2.6.4.1)[^s10]

---

## 9) 근거-파라미터 추적표

| Step B 항목 | 현재 정책값/규칙 | 근거 출처 | 근거 성격 |
|---|---|---|---|
| `T_warn` 기준축 | MID 2.0s | `>2s` inattentive 지속 위험 증가[^s1][^s2], NHTSA 2s 원칙[^s3][^s4], 100-Car 보고서[^s11] | 정량 위험 + 가이드라인 |
| speed band별 단축 | LOW > MID > HIGH | 맥락/시나리오/속도에 따라 주의여유 변화[^s5][^s6] | 맥락 의존 정책화 |
| `T_esc`, `T_absent` 단계 | 단계형 상승 | eyes-off와 lane-keeping 저하 연계[^s7], 경고 단계 시험체계[^s8][^s9][^s12] | 성능 저하 + 시험 구조 |
| critical reason 즉시 상향 | `unresponsive/intoxicated` 시 즉시 `ABSENT` | 응급/고위험 맥락 보수 운용 원칙 | 안전 우선 |
| `T_recover_hold` | OK 직접 복귀 | 최소 재참여 시간 요구(200ms)[^s10] | 재참여 안정화 |
| speed band 경계값 | `rho_v` band 경계 | 맥락 의존 연구 + 스케일카 정규화 전략[^s5][^s6] | 기준선 경계(B/C) |
| `T_warn_eff` 상한 | `<=5.0s` clamp | UNECE EOR 5s 앵커 보호[^s10] | 규정 방어 로직 |
| stale fail-safe | `OK->WARNING`, `WARNING+`는 freeze | 규정 경고전략 + 안전 우선[^s8][^s10] | 안전 원칙 |

---

## 10) 근거 강도 등급 및 확정/가정 분리

### 10.1 근거 강도 등급

- `A`: 규정 본문/공식 시험 절차/대규모 자연주의 데이터로 직접 뒷받침
- `B`: 동료심사 연구에서 일관 경향 확인(수치 직접 고정은 아님)
- `C`: 프로젝트 운영 가정(캘리브레이션 전제)

### 10.2 Step B 파라미터 분류

| 항목 | 분류 | 근거 강도 | 비고 |
|---|---|---|---|
| EOR 5s / DCA(+강도 증가) +3s / unavailability +10s / re-engagement 200ms | 참고값(규정앵커) | A | 설계 참고용. 구현은 본 문서 시간 테이블 우선 |
| `T_warn` MID=2.0s | 가정값(캘리브레이션) | B | 2s 위험 경계 신호 정합[^s1][^s2][^s3][^s11] |
| speed band 테이블 | 가정값(캘리브레이션) | B | 맥락 의존 연구 기반[^s5][^s6] |
| speed band 경계값(`rho_v`) | 가정값(캘리브레이션) | B | 스케일카 정규화 경계(변경 시 버전관리) |
| `T_esc`, `T_absent` | 가정값(캘리브레이션) | B | 단계형 경고 설계 + 시험체계 정합[^s7][^s12] |
| reason 기반 타이머 보정 | **미사용(v0 고정)** | C | 맥락은 즉시 상향/의미 신호로만 사용 |
| `T_warn_eff` 하한 | 가정값(캘리브레이션) | C | 운영 안정성 가정 |
| stale 시 `OK->WARNING`, `WARNING+` freeze | v1 규칙(프로토타입 v0 비활성) | B | 안전 우선 |

---

## 11) Step B 심화 조사 요약 (고신뢰 출처)

### 11.1 자연주의 데이터

- 100-Car/NTDS 계열에서 `2s` 이상 eyes-off 구간부터 위험 증가가 반복 관측[^s1][^s2][^s11]
- `LGOR`가 `TEOR`보다 실시간 위험 지표로 일관성이 높다는 보고[^s1][^s2]

### 11.2 주행 맥락 의존성

- 도심/농로/고속 시나리오, 속도, 환경 복잡도에 따라 attentional spare capacity 변화[^s5][^s6]

### 11.3 규정 및 시험 정합

- UNECE R171: 감지-경고-에스컬레이션-불가용 응답의 시간 구조 명시[^s10]
- Euro NCAP SD-201/202/Driver Engagement: warning timing 및 시험 재현성 앵커 제공[^s8][^s9][^s12]
- NHTSA: 2초/12초 가이드 및 시험절차 문서 제공[^s3][^s4][^s13]

### 11.4 운전자 맥락 분류 연구 (신규)

- 규정 앵커는 문서/코드/테스트에서 참고 근거로 관리하되, 구현 파라미터의 직접 고정값으로 취급하지 않는다
- 단, `T_warn_eff <= 5.0s` 상한은 예외적으로 유지한다
- `B/C` 항목은 플랫폼별 데이터로 재추정하고 버전 태깅
- 문서/코드 동일 파라미터 키(`T_warn_base`, `T_esc_base`, `T_absent_base`, `T_recover_hold`) 유지

---

## 12) 참고문헌

[^s1]: Simons-Morton et al., *Keep Your Eyes on the Road: Young Driver Crash Risk Increases According to Duration of Distraction*, J Adolesc Health (PMCID: PMC3999409). https://pmc.ncbi.nlm.nih.gov/articles/PMC3999409/
[^s2]: Liang et al., *How dangerous is looking away from the road?*, Human Factors, PMID: 23397818. https://pubmed.ncbi.nlm.nih.gov/23397818/
[^s3]: U.S. DOT Press Release, *U.S. DOT Releases Guidelines to Minimize In-Vehicle Distractions*. https://www.transportation.gov/briefing-room/us-dot-releases-guidelines-minimize-vehicle-distractions
[^s4]: NHTSA Guidance Documents (Distracted Driving). https://www.nhtsa.gov/laws-regulations/guidance-documents
[^s5]: Liu et al., *Attentional Demand as a Function of Contextual Factors in Different Traffic Scenarios*, Human Factors, PMID: 31424969. https://pubmed.ncbi.nlm.nih.gov/31424969/
[^s6]: Liu et al., *Drivers’ Attention Strategies before Eyes-off-Road in Different Traffic Scenarios*, IJERPH (PMCID: PMC8038146). https://pmc.ncbi.nlm.nih.gov/articles/PMC8038146/
[^s7]: Peng et al., *Driver’s lane keeping ability with eyes off road: Insights from a naturalistic study*, Accid Anal Prev, PMID: 22836114. https://pubmed.ncbi.nlm.nih.gov/22836114/
[^s8]: Euro NCAP, *SD-202 Driver Monitoring Test Procedure v1.1*. https://cdn.euroncap.com/cars/assets/sd_202_driver_monitoring_test_procedure_v11_58ce3b3a54.pdf
[^s9]: Euro NCAP, *SD-201 Driver Monitoring Dossier Guidance v1.1*. https://cdn.euroncap.com/cars/assets/sd_201_driver_monitoring_dossier_guidance_v11_4fbc6a9531.pdf
[^s10]: UNECE, *UN Regulation No. 171 (E/ECE/TRANS/505/Rev.3/Add.170)*, `DOCS/R171e.pdf` 5.5.4.2.5~5.5.4.2.6 조항.
[^s11]: Klauer et al., *The Impact of Driver Inattention on Near-Crash/Crash Risk: 100-Car Naturalistic Driving Study Data* (NHTSA, 2006). http://hdl.handle.net/10919/55090
[^s12]: Euro NCAP, *Safe Driving Driver Engagement Protocol v1.1*. https://cdn.euroncap.com/cars/assets/euro_ncap_protocol_safe_driving_driver_engagement_v11_a30e874152.pdf
[^s13]: NHTSA, *Visual-Manual Driver Distraction Guidelines Test Procedures* (DOT HS 812 739, 2019, ROSA P). https://rosap.ntl.bts.gov/view/dot/41935
[^s14]: Feng et al., *A Study on the Current Status of Mobile Phone Use While Driving* (2015), examining visual attention recovery times by distraction type
[^s15]: Connor et al., *The Role of Inattention in the Near-Crash/Crash Events* (100-Car Study, 2013), comparing drowsy vs phone distraction risk ratios
[^s16]: Vanlaar et al., *Drinking and Driving: Relative Risk of Motor Vehicle Crash Death by Alcohol Consumption* (NHTSA, 2009), BAC vs reaction time degradation
