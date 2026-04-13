# Scale Car Baseline Conversion

이 문서는 `DOCS/state-machine-Baseline.md`와 `DOCS/control-policy-Baseline.md`의
실차/규정 기준값을 **스케일카용 소프트웨어 파라미터**로 바꾸는 기준이다.

전제:

- 스케일카 제어 범위
  - `steering`: `-0.65 ~ +0.65`
  - `throttle`: `0.0 ~ 0.4`
- 위 범위는 **LKAS 주행 시 실제로 출력되는 스케일카 운용 범위**다.
- DCAS 런타임에서 이 값을 다시 하드웨어 스케일로 재변환하지 않는다(중복 스케일 금지).
- 현재 정책 결정: **Step C에서 조향 제한값은 사용하지 않는다.**
- 따라서 변환 대상은 `throttle_limit` 및 Step B 파라미터가 중심이다.

---

## 1) 변환 원칙

### 1.1 Step C(제어 제한) 변환

Step C는 `throttle_limit`만 제한한다.

```text
scale_throttle = baseline_throttle_ratio × 0.4
```

즉, 실차 baseline의 throttle 비율형 제한값을 스케일카 최대 스로틀(`0.4`)로 환산한다.

### 1.2 Step B(시간값) 변환

Step B 시간값(`T_warn`, `T_unresponsive`, `T_absent`, `T_recover`)은
단순 선형 비율로 곱하지 않는다.

근거:

- 시간 임계값은 인간 주의/경고 구조(규정 앵커, 인지 반응)와 강하게 연결됨
- 제어 범위(`0.65`, `0.4`)와 직접 비례 관계가 아님

따라서 방법은 다음이 적절하다.

1. baseline 시간값을 시작점으로 둔다.
2. 스케일카 주행 로그 기준으로 오탐/미탐을 보며 보정한다.
3. 보정은 speed band 기준으로만 조정한다.

---

## 2) Step C throttle 제한값 변환표

| DriverState | baseline `throttle_limit` ratio | scale-car `throttle_limit` |
|---|---:|---:|
| `OK` | `1.00` | `0.40` |
| `WARNING` | `0.60` | `0.24` |
| `UNRESPONSIVE` | `0.20` | `0.08` |
| `ABSENT` | `0.00` | `0.00` |

해석:

- `WARNING`: 감속 유도 단계
- `UNRESPONSIVE`: 강한 감속 단계
- `ABSENT`: 추진력 차단 단계

---

## 3) Step B 속도 밴드의 스케일카 변환

질문 포인트: Step B는 속도/곡률 맥락을 쓰는데, 스케일카 기준이 필요하다.

권장 방식은 **정규화 speed band**다.

1. 스케일카 직선 안전최대속도를 `V_safe_max`로 둔다.
2. 정규화 속도 `rho = v / V_safe_max`를 계산한다.
3. 밴드는 다음처럼 정의한다.

```text
LOW:  rho < 0.30
MID:  0.30 <= rho < 0.65
HIGH: rho >= 0.65
```

예시:

- `V_safe_max = 6.0 km/h`이면
  - `LOW < 1.8`
  - `MID 1.8 ~ 3.9`
  - `HIGH >= 3.9`

이 방식의 장점:

- 트랙/배터리/노면이 바뀌어도 밴드 기준이 깨지지 않음
- 임의 절대속도보다 일관된 기준 유지 가능

### 3.1 속도 추정 노이즈/채터링 대응 (필수)

스케일카는 속도 추정(IMU 적분/비전 오도메트리/스로틀 역산) 노이즈가 커서,
경계값(`0.30`, `0.65`) 근처에서 band가 왕복(chattering)할 수 있다.

따라서 band 판정은 아래 순서를 고정한다.

1. 정규화 원시값 계산: `rho_raw = v_est / V_safe_max`
2. LPF 적용:

```text
rho_f[k] = alpha * rho_f[k-1] + (1-alpha) * rho_raw[k]
```

권장 시작값: `alpha = 0.85 ~ 0.95` (`dt=50~100ms`)

3. 히스테리시스 경계 적용(예시):

```text
LOW -> MID : rho_f > 0.32
MID -> LOW : rho_f < 0.28
MID -> HIGH: rho_f > 0.67
HIGH -> MID: rho_f < 0.63
```

4. 최소 유지시간(dwell) 적용:
  - 조건이 `T_band_hold` 이상 유지될 때만 전환
  - 권장 시작값: `T_band_hold = 0.3s`

산업 적용 원칙:

- 위험도 상향(보수화)은 빠르게 적용
- 완화 하향은 느리게 적용(채터링 방지)
- `T_warn_eff` 등 유효 임계시간은 band 변경 시 즉시 점프하지 않고 완만 갱신

---

## 4) Step B 시간값 보정 절차(근거 부족 방지)

시간값을 임의로 바꾸지 않기 위한 최소 절차:

1. baseline 시간값으로 시작
2. 최소 3개 시나리오 로그 수집
   - 직선 eyes-off
   - 완만 커브 eyes-off
   - 재참여(eyes-on 복귀)
3. 지표 계산
   - 경고 너무 늦은 비율
   - 경고 너무 이른 비율
   - 불필요 경고 빈도
4. 밴드별 보정
   - 늦음 ↑ -> 해당 band `T_warn` 소폭 감소(예: 10%)
   - 이름 ↑ -> 해당 band `T_warn` 소폭 증가(예: 10%)
   - 복귀 흔들림 -> `T_recover` 증가

즉, 시간값은 "배율 변환"이 아니라 "로그 기반 보정"이 근거가 된다.

---

## 5) LKAS 파라미터 변환 여부

결론:

- **DCAS 튜닝 문맥에서는 LKAS 파라미터를 변환 대상에 넣지 않는다.**
- `kp`, `kd`, `throttle_base/min`, `steer_threshold/max`는 LKAS 제어 품질 튜닝 항목이고,
  DCAS Step B/C 변환의 필수 항목이 아니다.

즉,

- 이 문서의 필수 변환: `Step C throttle_limit`, `Step B speed band 기준`
- LKAS 파라미터: 별도 LKAS 튜닝 문서에서 관리

---

## 6) 최종 적용값(1차)

- 스케일카 범위
  - `steering`: `-0.65 ~ +0.65` (Step C 제한값으로 직접 사용하지 않음)
  - `throttle`: `0.0 ~ 0.4`

- Step C throttle 제한
  - `OK = 0.40`
  - `WARNING = 0.24`
  - `UNRESPONSIVE = 0.08`
  - `ABSENT = 0.00`

- Step B 시간값
  - baseline 시작값 유지 후 로그 기반 보정

이 파일은 "단순 비율 변환"을 최소화하고,
필요한 곳(종방향 제한)에만 쓰는 보수적 기준이다.
