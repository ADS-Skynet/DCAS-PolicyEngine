# State Machine v1 (Step B Baseline)

본 문서는 `DCAS-PolicyEngine`의 Step B(운전자 상태 전이) **기준선(Baseline)** 명세다.
핵심 목표는 다음 두 가지다.

1. 규정/연구 기반으로 재현 가능한 전이 로직 정의
2. 기준선(고정 앵커)과 튜닝 대상(캘리브레이션 항목) 명확 분리

제어 제한/출력 정책(Step C)은 본 문서 범위에 포함하지 않는다.

---

## 1) Step B 목적과 범위

- 목적: 운전자 참여 상태를 `OK -> WARNING -> UNRESPONSIVE -> ABSENT`로 전이
- 범위: 상태 계산(타이머, 임계값, 보정계수, 히스테리시스, fail-safe)
- 비범위: throttle/steer 제한, HMI 상세 정책, ABSENT 진입 시 MRM 활성화 (= Step C 문서)

---

## 2) 입력/출력 인터페이스

### 2.1 입력

- 필수 입력
  - `is_attentive` (`yes/no`)  
    - 인지팀에서 실시간으로 전송하는 운전자 집중 여부
  - `reason` (`phone | drowsy | unresponsive | intoxicated | none | unknown`)
    - **[신규]** 인지팀에서 집중 안 한 원인을 분류해 즉시 전송
    - 집중 안 한 순간 어떤 level의 경고라도 이 정보와 함께 전달
  - `jetracer_input_0_4`
    - JetRacer 런타임 입력값(범위 `0.0 ~ 0.4`)
    - 물리적 `km/h`가 아니라 주행 맥락용 정규화 입력이다
  - `lkas_steer_abs` (`|steering|`, 범위 `0.0 ~ 0.65`)
  - `current_state` (현재 운전자 상태)
- 선택 입력
  - `inattentive_elapsed`  
    - 외부에서 주는 값이 아니라 Step B 내부 누적 타이머(기본)
  - `warning_elapsed` (WARNING 상태 체류시간)
  - `input_stale` (센서 stale/파싱 실패 플래그)
  - `road_curvature`
    - LKAS가 제공하는 현재 주행 커브 맥락(있으면 사용, 없으면 `lkas_steer_abs`로 대체)
  - `recovery_eligibility` (**[신규]**, optional)
    - 맥락별 회복 가능성 (e.g., drowsy는 깨어남 신호, unresponsive는 불가능)

### 2.2 출력

- `next_state`
- `transition_reason`
- `warning_event` (`true/false`)  
  - `OK -> WARNING` 최초 진입 시 `true`
- `vlm_request_hint` (`none|request_context`)  
  - `warning_event=true`일 때 노트북 측 VLM 호출 트리거 힌트

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
  - 임계시간 계산(`T_warn_eff`, `T_unresp_eff`, `T_absent_eff`)은 안정화된 `speed_band`만 사용

- `speed_band`의 역할
  - 경고/복귀 기준을 직접 결정하는 값이 아니라, 현재 주행 맥락의 거칠기(level)를 나누는 보조 축이다.
  - 주행 입력이 강하고 조향 부하가 큰 구간은 더 보수적인 임계시간을 쓰기 위한 분류용이다.
  - 실제 판단의 주축은 여전히 `is_attentive`와 그 지속시간(`inattentive_elapsed` / `recover_elapsed`)이다.

- `steer_band`는 정규화 조향으로 정의
  - `rho_s = lkas_steer_abs / 0.65`
  - `LOW`: `rho_s < 0.30`
  - `MID`: `0.30 <= rho_s < 0.65`
  - `HIGH`: `rho_s >= 0.65`

초기 스케일카 권장:

- `V_safe_max_kmh`: 스케일카 직선 안전최대속도(측정값)
- `lkas_steer_abs`는 LKAS 주행 시 관측 범위 `0.0 ~ 0.65`를 사용

주의:

- speed/steer 경계값(`0.30`, `0.65`)은 캘리브레이션 항목이며, 변경 시 로그 근거를 함께 남긴다.

---

## 2.3 운전자 맥락 (Reason) 분류

**[신규 요구사항]** 인지팀이 제공하는 `reason` 필드를 Step B에 통합.
이는 **집중 안 한 원인**을 분류하여 상태 전이 타이밍을 상향 조정(보수화)할 근거 제공.

| Reason | 의미 | 특성 | 위험도(상대) | 회복 가능성 |
|---|---|---|---|---|
| `drowsy` | 졸음 운전 (눈 감김/PERCLOS) | 반응시간 ↑↑, 주의력 저하 | 3.0-11.0배 | ✅ 깨어남 신호 필요 |
| `phone` | 핸드폰 사용 (시각 이탈/조작) | 시각 이탈 4.7초, 수동 집중 | 1.3-4.3배 | ✅ 빠른 회복 |
| `unresponsive` | 의식 없음 (동공 고정/무반응) | 동공 반응 없음, 신체 긴장 상실 | 8.0-50.0배 | ❌ 회복 불가능 |
| `intoxicated` | 음주 운전 (반응시간/불안정) | 반응시간 +150ms, 조향 불안정 | 2.0-15.0배 | ⚠️ 긴 회복시간 (30min) |
| `none` | 정상 (집중 있음) | 기본 상태 | 1.0배 | - |
| `unknown` | 불명 | 기본값으로 처리 | 1.0배 | - |

**근거:**
- Drowsy: Simons-Morton et al. (PMCID: PMC3999409), NHTSA LTCCS
- Phone: NHTSA Distraction Guidelines, Feng et al. (2015)
- Unresponsive: 의료 응급의학 데이터, 의식 상실 분류
- Intoxicated: NHTSA DWI Studies, BAC vs 반응시간 연구

**설계 원칙:**
- `reason`이 제공되면, 기본 임계값 `T_warn/T_unresp/T_absent`에 **맥락 보정계수 k_reason** 적용
- 예: `T_warn_eff = T_warn_base * k_context(speed) * k_steer * k_reason(drowsy)`
- UNRESPONSIVE와 INTOXICATED는 긴급도가 높아서 타이머를 더 단축

---

## 3) 운전자 맥락별 상태 전이 매트릭스 (신규)

**[Pattern B: 계층 모델]** - 기본 4-state + 맥락 보정계수 적용

기존 Step B의 4가지 상태는 유지하되, **맥락별로 타이머와 회복 조건이 달라짐**.

### 3.1 맥락별 보정계수 (k_reason)

| Reason | k_reason | 근거 | 비고 |
|---|---|---|---|
| `drowsy` | 0.60 | 반응시간 2배↑, 자연주의 데이터 | T_warn 1.2배 단축 |
| `phone` | 1.20 | 시각 이탈 4.7초, 낮은 위험도 | T_warn 1.2배 연장 허용 |
| `unresponsive` | 0.30 | 의식 없음, 즉각 대응 필요 | T_warn 3.3배 단축 |
| `intoxicated` | 0.80 | 반응시간 +150ms, 높은 위험 | T_warn 1.25배 단축 |
| `none/unknown` | 1.00 | 기본값 | 변화 없음 |

**계산식:**
```
T_warn_eff = T_warn_base(speed_band) * k_context(steer) * k_reason(reason)
             ↑ 속도 대역별       ↑ 조향 부하        ↑ 맥락 보정 (신규)

예: MID band, DROWSY, 고 조향부하
T_warn_eff = 2.0s * 0.9 * 0.60 = 1.08s
```

### 3.2 맥락별 회복 조건 (T_recover)

| Reason | T_recover | 회복 신호 | 예외사항 |
|---|---|---|---|
| `drowsy` | 2.0s | `is_attentive=yes` (깨어남 확인) + 안정성 | 짧은 눈 깜빡임 반복 시 신뢰성 낮음 |
| `phone` | 0.84s | `is_attentive=yes` (즉시) | 빠른 회복, 기본값의 70% |
| `unresponsive` | ∞ | **회복 불가** | ABSENT → MRM 유지, 수동 복귀만 가능 |
| `intoxicated` | 3.0s | `is_attentive=yes` + 시간 경과 | 음주 영향 지속, 기본값의 2.5배 |

---

## 4) 기본 임계값 (기준선 1차값) - 기존

> 아래 값은 자연주의 연구 경향 + 규정 시한 구조를 반영한 기준선이며,
> 실제 배포값은 플랫폼별 캘리브레이션으로 확정한다.

| speed band | base `T_warn` | base `T_unresponsive` | base `T_absent` | `T_recover` |
|---|---:|---:|---:|---:|
| LOW | 3.0s | 6.0s | 10.0s | 1.0s |
| MID | 2.0s | 4.0s | 8.0s | 1.2s |
| HIGH | 1.5s | 3.0s | 6.0s | 1.5s |

---

## 5) 보정계수 및 적용식

> 아래 값은 자연주의 연구 경향 + 규정 시한 구조를 반영한 기준선이며,
> 실제 배포값은 플랫폼별 캘리브레이션으로 확정한다.

| speed band | base `T_warn` | base `T_unresponsive` | base `T_absent` | `T_recover` |
|---|---:|---:|---:|---:|
| LOW | 3.0s | 6.0s | 10.0s | 1.0s |
| MID | 2.0s | 4.0s | 8.0s | 1.2s |
| HIGH | 1.5s | 3.0s | 6.0s | 1.5s |

설명:

- 인지팀 입력은 `is_attentive` yes/no만 받는다.
- Step B 내부에서 `is_attentive=no`가 지속된 시간을 `inattentive_elapsed`로 누적한다.
- 전이는 `inattentive_elapsed`와 유효 임계시간 비교로 결정한다.

---

## 5) 보정계수 및 적용식

- 스티어 보정: `k_steer`
  - `LOW`: `1.00`
  - `MID`: `0.90`
  - `HIGH`: `0.80`
  - `road_curvature`가 있으면 `lkas_steer_abs`와 함께 커브 부하 추정에 사용한다.
- 맥락 보정: `k_context`
  - `LOW`: `1.00`
  - `MID`: `0.90`
  - `HIGH`: `0.80`

적용식:

- `T_warn_eff = T_warn_base(speed_band) * k_context * k_steer * k_reason`
- `T_unresp_eff = T_unresp_base(speed_band) * k_context * k_steer * k_reason`
- `T_absent_eff = T_absent_base(speed_band) * k_context * k_steer * k_reason`
- `T_recover_eff = T_recover_base(reason)` (맥락에 따라 별도 설정)

해석:

- `T_warn_eff`는 `OK -> WARNING`에만 사용한다.
- `WARNING` 이후에는 더 짧은 `T_warn_eff`를 다시 쓰는 것이 아니라, `T_unresp_eff`와 `T_absent_eff`를 사용한다.
- 따라서 `WARNING` 상태 자체에 `T_warn`를 다시 깎는 구조는 쓰지 않는다.

안전 하한:

- `T_warn_eff >= 1.0s`

규정 상한(하드 리미트):

- `T_warn_eff <= 5.0s` (UNECE R171 EOR 앵커 보호)
- 구현 권장: `T_warn_eff = clamp(T_warn_eff_raw, 1.0s, 5.0s)`

---

## 6) 전이 규칙 (결정 순서 고정) - 기존 + 맥락 통합

전이 우선순위는 상향(악화) 우선이며, 상태 우선순위는 `ABSENT > UNRESPONSIVE > WARNING > OK`이다.

| 현재 상태 | 조건 | 다음 상태 | 맥락별 보정 |
|---|---|---|---|
| OK | `inattentive_elapsed >= T_warn_eff` | WARNING | k_reason 적용 |
| WARNING | `inattentive_elapsed >= T_unresp_eff` | UNRESPONSIVE | k_reason 적용 |
| UNRESPONSIVE | `inattentive_elapsed >= T_absent_eff` | ABSENT | k_reason 적용 |
| WARNING/UNRESPONSIVE/ABSENT | `is_attentive=yes` 연속 유지 `>= T_recover_eff` | OK | reason 맞춤 회복 조건 |
| ANY | `input_stale=true` 또는 입력 누락 | stale fail-safe | 맥락 상관없이 최상 유지 |

**맥락별 회복 조건 상세:**

- `reason=drowsy`: `is_attentive=yes` + 안정성 확인 (짧은 깜빡임 반복 제외) → T_recover=2.0s
- `reason=phone`: `is_attentive=yes` 즉시 → T_recover=0.84s
- `reason=unresponsive`: **회복 불가능** (의식 없음) → `T_recover=∞` → ABSENT에서 MRM 유지, 운전자 개입 필요
- `reason=intoxicated`: `is_attentive=yes` + 음주 영향 지속시간 경과 (30분) → T_recover=3.0s
- `reason=none/unknown`: 기본값 사용

### 6.1 WARNING 시 노트북 VLM 호출 흐름

- `OK -> WARNING` 최초 진입 시:
  - `warning_event=true`
  - `vlm_request_hint=request_context`
  - 노트북은 이를 수신해 VLM 호출
- 노트북은 VLM 결과(`reason/confidence`)를 후행으로 Jetson에 전송
- Step B는 VLM 결과를 상태 전이의 필수 조건으로 사용하지 않으며,
  Step C/HMI 보강 입력으로만 사용한다.

즉, 상태 전이는 `is_attentive` 기반으로 항상 독립 동작하고,
VLM은 WARNING 이후 맥락 보강 계층이다.

### 6.1.1 EOR 해제 및 재참여 확인 기준

- EOR는 운전자가 다시 주행 작업 관련 영역으로 시선/머리 자세를 돌린 뒤,
  그 상태가 **최소 200ms 연속 유지**될 때만 해제(confirmed/clear)되는 것으로 본다.
- 따라서 알림을 끄는 조건은 "순간적인 복귀"가 아니라 `recover_elapsed >= 200ms`인 **안정적 재참여**이다.
- `recover_elapsed`가 200ms에 도달하기 전에 다시 시선/머리 자세가 이탈하면,
  누적은 끊기고 EOR는 유지된다.
- 짧은 이탈이 여러 번 반복되면, 제조업체 전략에 따라 다음 중 하나를 적용한다.
  - `T_recover`를 일시적으로 증가시켜 더 긴 안정 구간을 요구한다.
  - 또는 즉시/조기 EOR를 다시 발령하여 반복적인 짧은 복귀를 억제한다.

해석:

- 이 조항은 "알림을 최소 0.2초만 띄우고 끄라"는 뜻이 아니라,
  **재참여가 0.2초 이상 안정적으로 유지되어야 알림을 해제할 수 있다**는 뜻이다.
- 따라서 이 프로젝트에서는 `recover_elapsed`를 EOR 해제와 상태 복귀의 공통 검증 변수로 사용하되,
  경고 해제와 상태 전이는 분리해서 관리한다.

### 6.2 복귀 시 타이머 처리(명시)

복귀 시 `inattentive_elapsed`를 `is_attentive=yes`가 들어왔다고 바로 0으로 만들지 않는다.

권장 타이머 분리:

- `inattentive_elapsed`: `is_attentive=no`일 때만 누적
- `recover_elapsed`: `is_attentive=yes`가 연속 유지되는 시간 누적

처리 규칙:

- `is_attentive=no`가 들어오면 `recover_elapsed = 0`
- `is_attentive=yes`가 들어오면 `recover_elapsed`를 누적하고 `inattentive_elapsed`는 유지
- `recover_elapsed >= T_recover`가 되면 `next_state = OK`로 복귀하고 `inattentive_elapsed = 0`, `recover_elapsed = 0`

주의:

- HMI 버튼, 차량 actuation, 기타 비인지 입력은 `is_attentive`를 대체하지 않는다.
- 주의 상태는 오직 인지팀의 `is_attentive` 스트림으로만 판정한다.

의도:

- 짧은 eyes-on 반복으로 타이머를 악용해 경고를 우회하는 패턴(system abuse)을 방지

### 6.3 stale fail-safe 규칙(충돌 방지)

`input_stale=true` 또는 필수 입력 누락 시 다음 규칙을 적용한다.

- `current_state == OK` 이면 `next_state = WARNING`으로 강제 상향
- `current_state in {WARNING, UNRESPONSIVE, ABSENT}` 이면 **현재 상태 동결(freeze)**
- stale 동안 상태 하향(완화) 금지
- stale 해제 후에만 정상 전이/복귀 로직 재개

---

## 7) 임계 시간 산정 방법 (속도/스티어 기반, 상세)

질문 포인트:

- "차량 속도/스티어링에 따라 다음 상태로 넘어가는 inattentive 임계시간이 달라져야 한다"

타당한 방법은 아래 3단계다.

### 7.1 1단계: 기본축 고정 (human-factor 앵커)

- 기본축은 `speed_band`별 테이블을 사용한다.
- 이유:
  - 규정 구조(EOR/escalation/DCA/unavailability)가 시간 축으로 정의됨
  - 자연주의 연구에서 eyes-off 지속시간이 위험 증가와 직접 연관

즉, 기본 `T_warn/T_unresp/T_absent`는 사람 반응시간 앵커로 시작한다.

### 7.2 2단계: 스티어 부하 보정 (vehicle-context 보정)

- 스티어 절대값이 커질수록(코너/부하 증가) 임계시간을 짧게 만든다.
- `k_steer`를 곱해 보수화한다.

권장 초기값:

- `LOW`: `1.00`
- `MID`: `0.90`
- `HIGH`: `0.80`

해석:

- 조향 부하가 높은 구간에서는 inattentive 허용 시간을 10~20% 줄여 경고를 앞당긴다.

### 7.3 3단계: 로그 기반 밴드별 보정

임의값 방지를 위해 아래 지표로 조정한다.

- `Late Warning Rate` (위험 구간인데 경고가 늦은 비율)
- `Early Warning Rate` (불필요하게 이른 경고 비율)
- `Recovery Instability` (복귀 직후 재경고 반복)

보정 규칙(권장):

- `Late Warning Rate` 높음 -> 해당 band `T_warn_base` 10% 감소
- `Early Warning Rate` 높음 -> 해당 band `T_warn_base` 10% 증가
- 복귀 흔들림 높음 -> `T_recover` 0.2s 증가

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
- EOR 후 `3s` 이내 escalation 필요
- escalation 후 `5s` 이내 DCA 필요
- 첫 escalation 후 `10s` 이내 driver unavailability response 필요
- 재참여 판정 최소 지속시간 `>=200ms`

출처: `DOCS/R171e.pdf` (5.5.4.2.5.2.1, 5.5.4.2.6.2.1, 5.5.4.2.6.2.2, 5.5.4.2.6.3.1, 5.5.4.2.6.4.1)[^s10]

---

## 9) 근거-파라미터 추적표

| Step B 항목 | 현재 정책값/규칙 | 근거 출처 | 근거 성격 |
|---|---|---|---|
| `T_warn` 기준축 | MID 2.0s | `>2s` inattentive 지속 위험 증가[^s1][^s2], NHTSA 2s 원칙[^s3][^s4], 100-Car 보고서[^s11] | 정량 위험 + 가이드라인 |
| speed band별 단축 | LOW > MID > HIGH | 맥락/시나리오/속도에 따라 주의여유 변화[^s5][^s6] | 맥락 의존 정책화 |
| `T_unresponsive`, `T_absent` 단계 | 단계형 상승 | eyes-off와 lane-keeping 저하 연계[^s7], 경고 단계 시험체계[^s8][^s9][^s12] | 성능 저하 + 시험 구조 |
| `k_steer` | `LOW/MID/HIGH=1.0/0.9/0.8` | 고부하 상황에서 보수화 필요 경향[^s5][^s6] | 맥락 보정 |
| `T_recover` | OK 직접 복귀 | 최소 재참여 시간 요구(200ms)[^s10] | 재참여 안정화 |
| speed/steer 경계값 | `rho_v`, `rho_s` band 경계 | 맥락 의존 연구 + 스케일카 정규화 전략[^s5][^s6] | 기준선 경계(B/C) |
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
| EOR 5s / escalation +3s / DCA +5s / unavailability +10s / re-engagement 200ms | 확정값(규정앵커) | A | UNECE R171 조항[^s10] |
| `T_warn` MID=2.0s | 가정값(캘리브레이션) | B | 2s 위험 경계 신호 정합[^s1][^s2][^s3][^s11] |
| speed band 테이블 | 가정값(캘리브레이션) | B | 맥락 의존 연구 기반[^s5][^s6] |
| speed/steer 경계값(`rho_v`, `rho_s`) | 가정값(캘리브레이션) | B | 스케일카 정규화 경계(변경 시 버전관리) |
| `T_unresponsive`, `T_absent` | 가정값(캘리브레이션) | B | 단계형 경고 설계 + 시험체계 정합[^s7][^s12] |
| `k_steer` | 가정값(캘리브레이션) | B | 조향 부하 보수화 |
| `k_context`, `T_warn_eff` 하한 | 가정값(캘리브레이션) | C | 운영 안정성 가정 |
| k_reason (맥락 보정) | 가정값(캘리브레이션) | B | 운전자 맥락별 위험도 연구 기반[^s14][^s15][^s16] |
| stale 시 `OK->WARNING`, `WARNING+` freeze | 확정규칙(안전원칙) | B | 안전 우선 |

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

- `A` 등급 항목(규정 앵커)은 문서/코드/테스트에서 고정 관리
- `B/C` 항목은 플랫폼별 데이터로 재추정하고 버전 태깅
- 문서/코드 동일 파라미터 키(`T_warn_base`, `k_steer`, `T_recover`) 유지

---

## 13) 참고문헌

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
