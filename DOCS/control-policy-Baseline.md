# Control Policy Baseline (Step C)

본 문서는 `DCAS-PolicyEngine`의 Step C(정책 결정/제어 제한) **기준선(Baseline)** 명세다.
Step B 상태(`OK -> WARNING -> UNRESPONSIVE -> ABSENT`)를 입력받아,
제어 제한·HMI·비상 플래그를 일관되게 산출한다.

핵심 목표:

1. 규정 앵커(UNECE/Euro NCAP)와 충돌하지 않는 대응 정책 고정
2. 규정 고정값(A)과 캘리브레이션 항목(B/C) 분리

---

## 1) Step C 목적과 범위

- 목적:
  - 운전자 상태 악화 시 위험 완화(감속/경고 강화)
  - 재참여 유도(EOR/HOR/DCA 메시지 일관화)
  - Driver Unavailability 단계에서 최소위험 정지 방향으로 전환
- 범위:
  - `throttle_limit`, `hmi_action`, `emergency_flag`
  - 다중 시스템 경고 우선순위/충돌 해소
- 비범위:
  - 상태 전이 타이밍 계산(= Step B)
  - 저수준 제어기(PID/MPC) 내부 파라미터 튜닝

---

## 2) 입력/출력 인터페이스

### 2.1 입력

- `driver_state`: `OK | WARNING | UNRESPONSIVE | ABSENT`
- `reason`: `phone | drowsy | unresponsive | none | unknown`
- `lkas_throttle`: LKAS가 요청한 원시 종방향 제어값
- `input_stale`: 센서/파싱 stale
- `aeb_active` (optional): 긴급 제동/충돌회피 시스템 활성 상태

설계 원칙:

- 속도/곡률 맥락은 Step B 전이 임계값에서 이미 반영되므로,
  Step C는 **상태 기반 강도 정책**으로 단순화한다(이중 반영 방지).

### 2.2 출력

- `throttle_limit`
- `hmi_action`
- `emergency_flag`: `OFF | SOFT | HARD`

---

## 3) 규정/평가 앵커 (Step C 고정 제약)

| 앵커 | Step C에 필요한 해석 | 출처 |
|---|---|---|
| 시스템 제어는 충돌 위험을 줄이되 운전자 개입 가능해야 함 | 급격/과도한 제어를 피하고, 항상 운전자 개입 가능 상태 유지 | UNECE R171 5.3.4, 5.3.6.1[^c1] |
| 경계/종료 시 보조 기능은 controllable way로 종료 | `UNRESPONSIVE/ABSENT`에서도 종료·감속 전략은 조향 안정성 유지 | UNECE R171 5.3.5.2.1[^c1] |
| EOR는 시각 + 타 모달리티, DCA는 즉시 수동 인수 명령 | WARNING/UNRESPONSIVE의 HMI는 다중모달 구조로 고정 | UNECE R171 5.5.4.2.3.2~3[^c1] |
| 경고 에스컬레이션 시 emergency 시스템 경고 우선 고려 | AEBS 등과 동시 활성 시 HMI arbitration 필요 | UNECE R171 5.5.4.2.2.2[^c1] |
| Driver state에 따라 Forward/Lane support 민감도 증가 또는 EF 시작 | 상태 악화 시 제어 민감도 상향, EF는 별도 hard 단계로 연결 | Euro NCAP Driver Engagement 1.4.3[^c2] |
| EF는 distinct warning 시작 후 최대 5초 내 시작 | Step C의 `HARD` 진입 시점을 Step B 타이머와 동기화 | Euro NCAP Driver Engagement 1.4.3.3[^c2] |

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

| DriverState | throttle_limit | HMI | emergency_flag |
|---|---:|---|---|
| OK | `1.00 * lkas_throttle` | 상태표시만 | OFF |
| WARNING | `<= 0.60 * lkas_throttle` | EOR/HOR 에스컬레이션(반복 음향/햅틱) | OFF |
| UNRESPONSIVE | `<= 0.20 * lkas_throttle` | DCA + 카운트다운 | SOFT |
| ABSENT | `0.0` | Driver Unavailability Response(최대 경고) | HARD |

실행 규칙:

- 상위 상태 완화 금지: `ABSENT > UNRESPONSIVE > WARNING > OK`
- `HARD`면 항상 `throttle_limit = 0.0`
- 상태 전이로 `throttle_limit` 목표가 급변하면, 최종 인가값은 반드시 Rate Limiter 또는 LPF를 통과한다.
  - 권장 예: `|d(throttle_limit_cmd)/dt| <= 0.1 /s`
- `emergency_flag=HARD`이면 단순 `throttle=0`만으로 종료하지 않고, 가능 시 MRM 감속 모드(능동 제동)로 전환한다.
  - 권장 예: `a_mrm_cmd <= -2.0 m/s^2` (플랫폼/브레이크 HW capability에 맞춰 캘리브레이션)

---

## 5) Reason Overlay (상태 기반 정책 위에 덧씌움)

| reason | 적용 상태 | 제어값 영향 | HMI 문구/동작 |
|---|---|---|---|
| `phone` | WARNING+ | 제어값 추가 보수화 없음(Core 유지) | "전방주시 필요(휴대폰 감지)" + 비프 주기 단축 |
| `drowsy` | WARNING+ | `throttle_limit` 추가 10% 보수화 | "졸음 경고, 즉시 주시" + 휴식 유도 문구 |
| `unresponsive` | UNRESPONSIVE/ABSENT | Core 완화 금지 | "반응 없음, 감속/정지 유도" |
| `none/unknown` | 모든 상태 | Core만 적용 | 일반 eyes-on 경고 |

Overlay 불변식:

- Overlay는 상향 보수화만 허용(완화 금지)
- `input_stale=true`면 Overlay 비활성화 후 Core만 적용

---

## 6) HMI 에스컬레이션 시퀀스 (Step B 타이머와 결합)

이 섹션은 **상태 전이 사실을 운전자에게 알리고**, 단계별로 필요한 행동(주시 복귀/즉시 인수)을
명령형으로 전달하는 HMI 규칙이다.

| 단계 | Step B 상태 기준 | Step C HMI 요구 |
|---|---|---|
| Stage 0 | `OK` | 정보성 표시만 |
| Stage 1 | `WARNING` 초기 | EOR/HOR: 연속 시각 + 음향/햅틱 최소 1개 |
| Stage 2 | `WARNING` 지속 | 에스컬레이션: 강도 증가(반복/증폭) |
| Stage 3 | `UNRESPONSIVE` | DCA: "즉시 수동 인수" 명령형 경고 |
| Stage 4 | `ABSENT` | Driver Unavailability Response + 최대 경고 |

정합 조건:

- Stage 전환 시점은 Step B 규정 앵커(5s/3s/5s/10s)와 반드시 동기화한다.
- 경고 채널 충돌 시 emergency 시스템 경고를 우선 표시한다.

---

## 7) 충돌 해소(Arbitration) 및 Fail-safe

### 7.1 Emergency 시스템 우선순위

- `aeb_active=true`면:
  - Step C HMI를 보조 채널로 격하(메인 경고는 AEBS)
  - 종방향(`throttle_limit`)은 더 보수적인 값 선택(`min` 규칙)

### 7.2 stale/fault 처리

- `input_stale=true`이면:
  - 상태 완화 금지(상위 상태 유지)
  - Overlay 비활성화
  - 최소 `WARNING`급 HMI 유지

### 7.3 경계/종료 controllability

- 시스템 종료/경계 초과/오류 상황에서도 급격한 종방향 변화 금지
- 조향은 LKAS 원출력을 유지하고, DCAS는 종방향 제한 및 HMI/긴급 상태만 중재

---

## 8) 캘리브레이션 항목 (B/C) vs 고정 앵커 (A)

### 8.1 근거 강도 등급

- `A`: 규정 본문 또는 공식 시험 요건에 직접 명시
- `B`: 공식 프로토콜 + 연구 경향으로 방향성이 강함
- `C`: 플랫폼 제약 기반 운영 가정

### 8.2 분류표

| 항목 | 분류 | 근거 강도 | 비고 |
|---|---|---|---|
| 다중모달 EOR/DCA 요구 | 확정값 | A | UNECE 5.5.4.2.3.2~3[^c1] |
| 경고 충돌 시 emergency 우선 고려 | 확정값 | A | UNECE 5.5.4.2.2.2[^c1] |
| controllable termination/driver intervention 보장 | 확정값 | A | UNECE 5.3.5.2.1, 5.3.6.1[^c1] |
| EF 개입 5초 이내 시작 요구 | 확정값 | A | Euro NCAP 1.4.3.3[^c2] |
| 상태별 throttle 비율값 | 가정값 | C | JetRacer baseline, 실차별 재튜닝 |
| reason overlay 보수화 비율(5%, 10%) | 가정값 | C | 운영 안정성 가정 |
| 상태 전이 시 Rate Limiter/LPF | 가정값(권장 규칙) | B | UNECE controllability/driver intervention 취지 정합[^c1] |
| `HARD` 시 능동 제동(MRM 감속) | 가정값(권장 규칙) | C | 규정은 응답 요구, 감속도 수치(`-2.0`)는 플랫폼 캘리브레이션 |

---

## 9) 구현 인터페이스 권장

```text
PolicyOutput evaluate_policy(
  DriverState driver_state,
  Reason reason,
  float lkas_throttle,
  bool input_stale,
  bool aeb_active
)
```

반환:

```text
{ throttle_limit, hmi_action, emergency_flag }
```

권장 단위 테스트:

- 상태 단조성: 상위 상태에서 제한이 완화되지 않음
- arbitration: `aeb_active=true` 시 우선순위/`min` 규칙 검증
- stale: 완화 금지 + WARNING 이상 HMI 유지

---

## 10) 참고문헌

[^c1]: UNECE, *UN Regulation No. 171 (E/ECE/TRANS/505/Rev.3/Add.170)*, `DOCS/R171e.pdf` (5.3.4, 5.3.5.2.1, 5.3.6.1, 5.5.4.2.2.2, 5.5.4.2.3.2~3).
[^c2]: Euro NCAP, *Safe Driving Driver Engagement Protocol v1.1* (2025), section 1.4.1~1.4.3.3. https://cdn.euroncap.com/cars/assets/euro_ncap_protocol_safe_driving_driver_engagement_v11_a30e874152.pdf
[^c3]: Peng et al., *Driver’s lane keeping ability with eyes off road: Insights from a naturalistic study*, Accid Anal Prev, PMID: 22836114. https://pubmed.ncbi.nlm.nih.gov/22836114/
[^c4]: Liang et al., *How dangerous is looking away from the road?*, Human Factors, PMID: 23397818. https://pubmed.ncbi.nlm.nih.gov/23397818/
