# DCAS PolicyEngine Q&A (2026-04-23)

요청하신 항목을 기준으로, `state-machine`/`control-policy` 두 문서에 반영된 현재 계약을 정리합니다.

---

## 1) Step B 내부 계산 vs 인지팀 입력 경계

질문:
- `current_state`, `inattentive_elapsed_s`, `recover_elapsed_s`, `t_warn_eff_s`, `t_esc_eff_s`, `t_absent_eff_s`, `t_recover_hold_s`를 Step B 내부에서 정/계산해야 하는가?
- 인지팀 노트북 입력은 `is_attentive`, `reason`만 맞는가?

답변:
- 네, 방향이 맞습니다. 인지팀의 직접 입력 계약은 `is_attentive`, `reason`(+ 각 timestamp)로 두고,
  Step B가 상태/타이머/전이를 내부에서 관리하는 구조가 맞습니다.
- `T_*` 값은 최종적으로 Step B 내부 하위 모듈(`speed_band`/임계값 스케줄러)에서 계산하는 구조를 권장합니다.

권장 인지팀 입력 계약:
- `is_attentive`, `is_attentive_ts_ms`
- `reason`, `reason_ts_ms`

---

## 2) Recover 200ms 구간에서 Step C 경고 종료 구현 여부

질문:
- `if (recover_elapsed_s >= 0.2) { 상태 유지 }`이면 Step C 경고 채널이 완화되는가?

답변:
- 현재 구현 기준으로는 아직 아닙니다.
- Step B는 상태 유지만 하고, Step C는 상태 기반(`WARNING->EOR`, `ESCALATION->DCA`)으로 경고를 계속 출력합니다.
- "상태 유지 + 경고 채널만 완화"를 하려면 별도 신호(예: `eor_clear_confirmed`)를 Step B->Step C 인터페이스에 추가해야 합니다.

---

## 3) Stale Fail-Safe 문서/구현 정합

질문:
- 문서에는 프로토타입에서 stale 미사용 아닌가?

답변:
- 맞습니다.
- 두 문서 모두 프로토타입 v0에서는 stale 분기 비활성(예약)으로 정의합니다.

정합 권고:
- v0 strict: stale 분기 비활성화
- v1 safety: stale 분기 활성화
- 빌드/런타임 설정으로 분기 관리

---

## 4) is_attentive / critical / timestamp 규칙 반영 결과 (두 문서 공통)

요청 의도:
- critical reason 이후 동일 run cycle에서 `OK` 복귀 금지
- 항상 동일 timestamp의 `is_attentive`/`reason` 쌍으로만 판단

반영 결과 (`DOCS/state-machine-Baseline.md`):
- `critical reason`으로 `ABSENT` 강제 시 `absent_latched_run_cycle=true`
- 같은 run cycle에서는 이후 `is_attentive=yes`가 들어와도 복귀 판단에 사용하지 않음
- 전이 판단은 `is_attentive_ts_ms == reason_ts_ms`인 동일 snapshot만 유효
- timestamp 불일치 snapshot은 전이 판단에서 제외(상태 유지/래치 우선)

반영 결과 (`DOCS/control-policy-Baseline.md`):
- Step C는 Step B가 동일 snapshot에서 정규화한 `reason`만 유효 입력으로 소비
- timestamp 불일치로 제외된 지연 `reason`은 Step C가 재해석하지 않음
- Step B 래치로 `ABSENT`가 유지되는 run cycle에서는 Step C도 하향 해석 없이 run-cycle 정책을 따름

---

## 5) "입력" 오해 해소 + 유지보수형 구현 구조 계획

핵심:
- 인지팀 입력은 `is_attentive/reason(+ts)`로 단순화
- `T_*`는 Step B 내부 계산 산출물로 일원화

권장 모듈 구조:
1. `PerceptionAdapter`
   - 입력: `is_attentive`, `is_attentive_ts_ms`, `reason`, `reason_ts_ms`
   - 출력: `NormalizedSnapshot`
   - 책임: 동일 timestamp 결합 + 정규화

2. `StateTimerStore`
   - 상태/타이머 저장: `current_state`, `inattentive_elapsed_s`, `recover_elapsed_s`, `absent_latched_run_cycle`

3. `SpeedBandEstimator`
   - LPF + hysteresis + dwell로 `speed_band` 산출

4. `ThresholdScheduler`
   - `speed_band -> t_warn_eff_s, t_esc_eff_s, t_absent_eff_s, t_recover_hold_s`

5. `StepBTransitionEngine`
   - snapshot/타이머/임계값 기반 전이 결정

6. `StepCPolicyEngine`
   - `StepBOutput` 기반 정책(`throttle_limit`, `hmi_action`, `mrm_active`, `driver_override_lock`) 결정

---

## 추가 메모

- 이번 작업은 문서 반영만 수행했고, 구현 코드는 변경하지 않았습니다.
- 반영 문서:
  - `DOCS/state-machine-Baseline.md`
  - `DOCS/control-policy-Baseline.md`
- 다음 구현 단계 우선순위:
  - `PerceptionAdapter` 도입으로 동일 timestamp 결합 규칙 + critical ABSENT latch 규칙을 코드에서 강제
