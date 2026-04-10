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
- 비범위: throttle/steer 제한, HMI 상세 정책(별도 Step C 문서)

---

## 2) 입력/출력 인터페이스

### 2.1 입력

- 필수 입력
  - `eyes_off_elapsed`
  - `speed_band` (`LOW`/`MID`/`HIGH`)
  - `curvature_band` (`LOW`/`HIGH`)
  - `current_state`
- 선택 입력
  - `warning_elapsed`
  - `driver_absent_elapsed`
  - `input_stale` (센서 stale/파싱 실패 플래그)

### 2.2 출력

- `next_state`
- `transition_reason`

---

## 3) 기본 임계값 (기준선 1차값)

> 아래 값은 자연주의 연구 경향 + 규정 시한 구조를 반영한 기준선이며,
> 실제 배포값은 플랫폼별 캘리브레이션으로 확정한다.

| speed band | base `T_warn` | base `T_unresponsive` | base `T_absent` | `T_recover` |
|---|---:|---:|---:|---:|
| LOW | 3.0s | 6.0s | 10.0s | 1.0s |
| MID | 2.0s | 4.0s | 8.0s | 1.2s |
| HIGH | 1.5s | 3.0s | 6.0s | 1.5s |

---

## 4) 보정계수 및 적용식

- 곡률 보정: `k_curve = 0.8` (HIGH curvature), 아니면 `1.0`
- 상태 보정: 이미 `WARNING`이면 `k_state = 0.85`, 아니면 `1.0`

적용식:

- `T_warn_eff = T_warn_base(speed_band) * k_curve * k_state`
- `T_unresp_eff = T_unresp_base(speed_band) * k_curve`
- `T_absent_eff = T_absent_base(speed_band) * k_curve`

안전 하한:

- `T_warn_eff >= 1.0s`

---

## 5) 전이 규칙 (결정 순서 고정)

전이 우선순위는 상향(악화) 우선이며, 상태 우선순위는 `ABSENT > UNRESPONSIVE > WARNING > OK`이다.

| 현재 상태 | 조건 | 다음 상태 |
|---|---|---|
| OK | `eyes_off_elapsed >= T_warn_eff` | WARNING |
| WARNING | `eyes_off_elapsed >= T_unresp_eff` | UNRESPONSIVE |
| UNRESPONSIVE | `eyes_off_elapsed >= T_absent_eff` | ABSENT |
| WARNING/UNRESPONSIVE/ABSENT | `eyes_on`이 `T_recover` 이상 연속 유지 | 한 단계 하향 |
| ANY | `input_stale=true` 또는 입력 누락 | 최소 `WARNING` 유지 (fail-safe) |

보조 규칙:

- 단일 프레임 `eyes_on`으로 즉시 복귀 금지
- 복귀는 항상 단계적으로만 수행

---

## 6) UNECE R171 정합 앵커 (고정)

다음 항목은 Step B 기준선의 규정 앵커다.

- `>10 km/h`에서 시각 disengagement `5s` 지속 시 EOR 필요
- EOR 후 `3s` 이내 escalation 필요
- escalation 후 `5s` 이내 DCA 필요
- 첫 escalation 후 `10s` 이내 driver unavailability response 필요
- 재참여 판정 최소 지속시간 `>=200ms`

출처: `DOCS/R171e.pdf` (5.5.4.2.5.2.1, 5.5.4.2.6.2.1, 5.5.4.2.6.2.2, 5.5.4.2.6.3.1, 5.5.4.2.6.4.1)[^s10]

---

## 7) 근거-파라미터 추적표

| Step B 항목 | 현재 정책값/규칙 | 근거 출처 | 근거 성격 |
|---|---|---|---|
| `T_warn` 기준축 | MID 2.0s | `>2s` eyes-off 위험 증가[^s1][^s2], NHTSA 2s 원칙[^s3][^s4], 100-Car 보고서[^s11] | 정량 위험 + 가이드라인 |
| speed band별 단축 | LOW > MID > HIGH | 맥락/시나리오/속도에 따라 주의여유 변화[^s5][^s6] | 맥락 의존 정책화 |
| `T_unresponsive`, `T_absent` 단계 | 단계형 상승 | eyes-off와 lane-keeping 저하 연계[^s7], 경고 단계 시험체계[^s8][^s9][^s12] | 성능 저하 + 시험 구조 |
| `k_curve` | `0.8` | 고부하 상황에서 보수화 필요 경향[^s5][^s6] | 맥락 보정 |
| `k_state` | `0.85` | 지속 disengagement 빠른 대응 필요(운영 가정)[^s8][^s10] | 운영 설계 |
| `T_recover` | 단계적 복귀 | 최소 재참여 시간 요구(200ms)[^s10] | 재참여 안정화 |
| stale fail-safe | 최소 WARNING | 규정 경고전략 + 안전 우선[^s8][^s10] | 안전 원칙 |

---

## 8) 근거 강도 등급 및 확정/가정 분리

### 8.1 근거 강도 등급

- `A`: 규정 본문/공식 시험 절차/대규모 자연주의 데이터로 직접 뒷받침
- `B`: 동료심사 연구에서 일관 경향 확인(수치 직접 고정은 아님)
- `C`: 프로젝트 운영 가정(캘리브레이션 전제)

### 8.2 Step B 파라미터 분류

| 항목 | 분류 | 근거 강도 | 비고 |
|---|---|---|---|
| EOR 5s / escalation +3s / DCA +5s / unavailability +10s / re-engagement 200ms | 확정값(규정앵커) | A | UNECE R171 조항[^s10] |
| `T_warn` MID=2.0s | 가정값(캘리브레이션) | B | 2s 위험 경계 신호 정합[^s1][^s2][^s3][^s11] |
| speed band 테이블 | 가정값(캘리브레이션) | B | 맥락 의존 연구 기반[^s5][^s6] |
| `T_unresponsive`, `T_absent` | 가정값(캘리브레이션) | B | 단계형 경고 설계 + 시험체계 정합[^s7][^s12] |
| `k_curve` | 가정값(캘리브레이션) | B | 곡선/맥락 보수화 |
| `k_state`, `T_warn_eff` 하한 | 가정값(캘리브레이션) | C | 운영 안정성 가정 |
| stale 시 최소 WARNING | 확정규칙(안전원칙) | B | 안전 우선 |

---

## 9) Step B 심화 조사 요약 (고신뢰 출처)

### 9.1 자연주의 데이터

- 100-Car/NTDS 계열에서 `2s` 이상 eyes-off 구간부터 위험 증가가 반복 관측[^s1][^s2][^s11]
- `LGOR`가 `TEOR`보다 실시간 위험 지표로 일관성이 높다는 보고[^s1][^s2]

### 9.2 주행 맥락 의존성

- 도심/농로/고속 시나리오, 속도, 환경 복잡도에 따라 attentional spare capacity 변화[^s5][^s6]

### 9.3 규정 및 시험 정합

- UNECE R171: 감지-경고-에스컬레이션-불가용 응답의 시간 구조 명시[^s10]
- Euro NCAP SD-201/202/Driver Engagement: warning timing 및 시험 재현성 앵커 제공[^s8][^s9][^s12]
- NHTSA: 2초/12초 가이드 및 시험절차 문서 제공[^s3][^s4][^s13]

---

## 10) 운영 원칙 (기준선 유지 규칙)

- `A` 등급 항목(규정 앵커)은 문서/코드/테스트에서 고정 관리
- `B/C` 항목은 플랫폼별 데이터로 재추정하고 버전 태깅
- 문서/코드 동일 파라미터 키(`T_warn_base`, `k_curve`, `T_recover`) 유지

---

## 11) 참고문헌

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
