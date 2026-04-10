# DCAS 임계값/매핑 테이블 정리 (2026-04-10)

본 문서는 지금까지 조사한 내용과 추가 조사 결과를 바탕으로,
`DriverState × 주행상태(speed/curvature) × 맥락(reason)` 기반 매핑 테이블을 정리한 초안이다.

- 적용 범위: `DCAS-PolicyEngine` 정책 판단 레이어
- 비범위: 규제 인증용 최종 수치 확정(실차/시뮬레이터 캘리브레이션 전)
- 시스템 제약 반영: 현재 브레이크 하드웨어 없음(긴급 시 `throttle=0` 중심)

---

## 1) 조사 요약 (핵심 결론)

| 항목 | 조사 결과 | 정책 반영 포인트 |
|---|---|---|
| 시선 이탈 지속시간 | 시선이탈(특히 single longest glance) 시간이 길수록 crash/near-crash 위험이 단조 증가하며, `>2s` 구간부터 위험 증가가 뚜렷함[^1][^2] | `T_warn`의 기준축을 `2s` 근처로 두고, 고위험 조건에서 더 단축 |
| 속도/맥락 영향 | 주행 속도, 도로 시나리오(도심/농로/고속) 및 맥락 변수에 따라 attentional spare capacity가 달라짐[^3][^4] | 고속/고곡률일수록 동일 state에서도 임계값을 더 짧게 |
| 차선유지 악화 | eyes-off-road는 속도/차로폭 통제 후에도 SDLP 증가(차선유지 성능 저하)와 연관[^5] | `UNRESPONSIVE/ABSENT`에서 steer/throttle 제한 강도 강화 |
| HMI 설계 기준 | NHTSA 가이드 커뮤니케이션에서 “2초 단일, 12초 총량” 원칙 제시[^6][^7] | HMI 경고 escalation 설계의 상한 기준으로 사용 |
| Euro NCAP 원문 기준 | `SD-201/SD-202` 및 Driver Engagement 프로토콜에서 Driver Monitoring warning timing/시나리오/개입 평가 절차를 명시[^8][^9][^10] | Step B 전이 타이머와 Step C 경고 강도 매핑의 시험가능성(검증 항목) 근거로 사용 |
| UNECE 공식 근거 | UNECE R171 본문에서 driver disengagement 감지/경고 에스컬레이션(EOR/DCA/driver unavailability response) 요구사항을 명시[^11][^12] | Step B 타이머/상태 전이 상한선(규정 준수 관점)과 Step C 경고 강도 단계 설계 근거 |

---

## 2) 매핑 테이블 설계 방식 (권장)

### 2.1 2계층 구조

1. **Core Table**: `DriverState × speed_band × curvature_band -> 제어/HMI 액션`
2. **Reason Overlay**: `reason(phone/drowsy/unresponsive/none)`로 메시지/강도 미세조정

이 구조를 쓰면 인지팀 세부 confidence 모델이 고정되지 않아도, Core safety는 먼저 동작하고 reason은 후행 보강 가능하다.

### 2.2 입력 변수 정규화

- `speed_band`
  - `LOW`: `< 10 km/h`
  - `MID`: `10–25 km/h`
  - `HIGH`: `>= 25 km/h`
- `curvature_band`
  - `LOW`: `|curvature| < C1`
  - `HIGH`: `|curvature| >= C1`
- `DriverState`: `OK`, `WARNING`, `UNRESPONSIVE`, `ABSENT`

---

## 3) 임계값 테이블 (eyes-off 기준, 초안)

> 아래 수치는 “데모/통합 1차값”이며, `state-machine-v1.md` 확정 전 캘리브레이션 대상.

| speed band | base `T_warn` | base `T_unresponsive` | base `T_absent` | 복귀 지연(`T_recover`) |
|---|---:|---:|---:|---:|
| LOW | 3.0s | 6.0s | 10.0s | 1.0s |
| MID | 2.0s | 4.0s | 8.0s | 1.2s |
| HIGH | 1.5s | 3.0s | 6.0s | 1.5s |

### 3.1 보정계수

- 곡률 보정: `k_curve = 0.8` (HIGH curvature), 아니면 `1.0`
- 상태 보정: 이미 `WARNING`이면 추가 보수화 `k_state = 0.85`
- 최종 적용:

`T_warn_eff = T_warn_base(speed_band) * k_curve * k_state`

`T_unresp_eff = T_unresp_base(speed_band) * k_curve`

> 안전 하한 권장: `T_warn_eff >= 1.0s` (노이즈/오탐 방지용)

### 3.2 UNECE R171 정합 체크 (본문 PDF 기준)

- `>10 km/h`에서 시각 disengagement가 `5s` 지속되면 EOR를 **최대 시한 내** 발행해야 함(5.5.4.2.6.2.1)[^12]
- EOR 이후 disengagement가 계속되면 `3s` 이내 에스컬레이션(EOR 강도 증가, 음향/촉각 포함)(5.5.4.2.6.2.2)[^12]
- 에스컬레이션 EOR 이후 `5s` 이내 DCA(즉시 제어 복귀 요구) 발행(5.5.4.2.6.3.1)[^12]
- 첫 에스컬레이션 이후 최대 `10s` 내 driver unavailability response 시작(5.5.4.2.6.4.1)[^12]
- 시각 재참여 판정 최소 지속시간 `>=200ms` 요구(5.5.4.2.5.2.1)[^12]

> 메모: 본 문서의 `T_warn` 데모값(예: MID 2.0s)은 UNECE의 “최대 허용 경고 지연”보다 보수적인 내부 정책값으로 운용 가능하다. 단, 형식승인 관점에서는 EOR/DCA/unavailability 단계와 시한을 별도 검증 항목으로 관리해야 한다.

---

## 4) Core Action Mapping Table

| DriverState | speed/curvature | throttle_limit | steer_limit | HMI | emergency flag |
|---|---|---:|---:|---|---|
| OK | 모든 조건 | LKAS 값 유지 | LKAS 값 유지 | 없음/상태표시 | OFF |
| WARNING | LOW & low-curv | `<= 0.75 * lkas_throttle` | `<= 0.90 * lkas_steer` | 시각 경고 + 단일 비프 | OFF |
| WARNING | MID/HIGH 또는 high-curv | `<= 0.60 * lkas_throttle` | `<= 0.80 * lkas_steer` | 시각 경고 + 반복 비프 | OFF |
| UNRESPONSIVE | LOW | `<= 0.35 * lkas_throttle` | `<= 0.70 * lkas_steer` | 강한 경고 + 카운트다운 | ON(soft) |
| UNRESPONSIVE | MID/HIGH 또는 high-curv | `<= 0.20 * lkas_throttle` | `<= 0.60 * lkas_steer` | 강한 경고 + 카운트다운 | ON(soft) |
| ABSENT | 모든 조건 | `0.0` | `<= 0.50 * lkas_steer` | 최대 경고(정지 유도) | ON(hard) |

### 4.1 정책 의도

- `ABSENT > UNRESPONSIVE > WARNING > OK` 우선순위 고정
- 상위 상태가 하위 상태/overlay로 완화되지 않음
- 브레이크 미장착 제약을 반영해 긴급 대응은 `throttle=0` 중심

---

## 5) Reason Overlay Table (메시지/강도 보강)

| reason | 적용 상태 | 제어값 영향 | HMI 문구/동작 |
|---|---|---|---|
| `phone` | WARNING+ | 기본 제한 + `steer_limit` 추가 5% 보수화 | “전방주시 필요(휴대폰 감지)” + 비프 주기 단축 |
| `drowsy` | WARNING+ | 기본 제한 + `throttle_limit` 추가 10% 보수화 | “졸음 경고, 즉시 주시” + 단계 상승 |
| `unresponsive` | UNRESPONSIVE/ABSENT | Core와 동일(완화 금지) | “반응 없음, 감속/정지 유도” |
| `none/unknown` | 모든 상태 | Core만 적용 | 일반 eyes-on 경고 |

---

## 6) 운영 규칙 (stale/히스테리시스)

- VLM stale(`latency_ms` 초과, `parse_ok=false`) 시 overlay 비활성, Core만 유지
- 상태 상승(악화)은 빠르게, 복귀는 느리게(`T_recover` 유지 후 하향)
- 단일 프레임 eyes-on으로 즉시 `UNRESPONSIVE -> WARNING`, `WARNING -> OK` 복귀 금지

---

## 7) 검증 체크리스트 (테이블 확정 전)

1. **경계값 테스트**: `T_warn_eff` 직전/직후 전이 일관성
2. **속도 단조성**: `LOW -> MID -> HIGH`에서 임계값 비증가 확인
3. **곡률 단조성**: high-curv가 항상 더 보수적임을 확인
4. **상태 우선순위**: overlay가 상위 상태를 완화하지 못함을 확인
5. **Fail-safe**: 입력 누락/stale 시 최소 안전 동작(`WARNING` 이상 유지) 확인

---

## 8) 각주 (출처)

[^1]: Simons-Morton et al., *Keep Your Eyes on the Road: Young Driver Crash Risk Increases According to Duration of Distraction*, J Adolesc Health (PMCID: PMC3999409). https://pmc.ncbi.nlm.nih.gov/articles/PMC3999409/
[^2]: Liang et al., *How dangerous is looking away from the road?*, Human Factors, PMID: 23397818. https://pubmed.ncbi.nlm.nih.gov/23397818/
[^3]: Liu et al., *Attentional Demand as a Function of Contextual Factors in Different Traffic Scenarios*, Human Factors, PMID: 31424969. https://pubmed.ncbi.nlm.nih.gov/31424969/
[^4]: Liu et al., *Drivers’ Attention Strategies before Eyes-off-Road in Different Traffic Scenarios*, IJERPH (PMCID: PMC8038146). https://pmc.ncbi.nlm.nih.gov/articles/PMC8038146/
[^5]: Peng et al., *Driver’s lane keeping ability with eyes off road: Insights from a naturalistic study*, Accid Anal Prev, PMID: 22836114. https://pubmed.ncbi.nlm.nih.gov/22836114/
[^6]: U.S. DOT Press Release, *U.S. DOT Releases Guidelines to Minimize In-Vehicle Distractions* (2초 단일/12초 총량 문구 포함). https://www.transportation.gov/briefing-room/us-dot-releases-guidelines-minimize-vehicle-distractions
[^7]: NHTSA Guidance Documents, Distracted Driving 섹션(Visual-Manual Driver Distraction Guidelines 링크 포함). https://www.nhtsa.gov/laws-regulations/guidance-documents
[^8]: Euro NCAP, *SD-202 Driver Monitoring Test Procedure v1.1*. https://cdn.euroncap.com/cars/assets/sd_202_driver_monitoring_test_procedure_v11_58ce3b3a54.pdf
[^9]: Euro NCAP, *SD-201 Driver Monitoring Dossier Guidance v1.1*. https://cdn.euroncap.com/cars/assets/sd_201_driver_monitoring_dossier_guidance_v11_4fbc6a9531.pdf
[^10]: Euro NCAP, *Safe Driving Driver Engagement Protocol v1.1*. https://cdn.euroncap.com/cars/assets/euro_ncap_protocol_safe_driving_driver_engagement_v11_a30e874152.pdf
[^11]: United Nations Treaty Collection, *UN Regulation No. 171 (Driver Control Assistance Systems) entry/status record*. https://treaties.un.org/doc/Publication/MTDSG/Volume%20I/Chapter%20XI/XI-B-16-171.en.pdf
[^12]: UNECE, *UN Regulation No. 171 (E/ECE/TRANS/505/Rev.3/Add.170)*, 본문 PDF(`DOCS/R171e.pdf`) 확인. 핵심 조항: 5.5.4.2.5.2.1(재참여 200ms), 5.5.4.2.6.2.1(EOR 5s), 5.5.4.2.6.2.2(EOR escalation +3s), 5.5.4.2.6.3.1(DCA +5s), 5.5.4.2.6.4.1(unavailability response +10s).

> 참고: Euro NCAP 원문(PDF)은 접근 가능하며, UNECE는 `DOCS/R171e.pdf` 본문 기준으로 조항을 직접 반영했다. 대외 링크 추적은 UN Treaty Collection 항목을 함께 유지한다.
