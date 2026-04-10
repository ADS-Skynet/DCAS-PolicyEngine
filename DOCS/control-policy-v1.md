# Control Policy v1 (Step C)

본 문서는 Step C(정책 결정/제어 제한) 전용 문서다.
Step B에서 계산된 상태를 입력받아 throttle/steer 제한 및 HMI 대응을 결정한다.

---

## 1) Step C 정의

- 입력:
  - `driver_state` (from Step B)
  - `reason` (`phone`/`drowsy`/`unresponsive`/`none`)
  - `vehicle_speed`, `road_curvature`
  - `lkas_throttle`, `lkas_steer`
- 출력:
  - `throttle_limit`
  - `steer_limit`
  - `hmi_action`
  - `emergency_flag`

---

## 2) Core Action Mapping

| DriverState | speed/curvature | throttle_limit | steer_limit | HMI | emergency flag |
|---|---|---:|---:|---|---|
| OK | 모든 조건 | LKAS 값 유지 | LKAS 값 유지 | 없음/상태표시 | OFF |
| WARNING | LOW & low-curv | `<= 0.75 * lkas_throttle` | `<= 0.90 * lkas_steer` | 시각 경고 + 단일 비프 | OFF |
| WARNING | MID/HIGH 또는 high-curv | `<= 0.60 * lkas_throttle` | `<= 0.80 * lkas_steer` | 시각 경고 + 반복 비프 | OFF |
| UNRESPONSIVE | LOW | `<= 0.35 * lkas_throttle` | `<= 0.70 * lkas_steer` | 강한 경고 + 카운트다운 | ON(soft) |
| UNRESPONSIVE | MID/HIGH 또는 high-curv | `<= 0.20 * lkas_throttle` | `<= 0.60 * lkas_steer` | 강한 경고 + 카운트다운 | ON(soft) |
| ABSENT | 모든 조건 | `0.0` | `<= 0.50 * lkas_steer` | 최대 경고(정지 유도) | ON(hard) |

---

## 3) Reason Overlay

| reason | 적용 상태 | 제어값 영향 | HMI 문구/동작 |
|---|---|---|---|
| `phone` | WARNING+ | 기본 제한 + `steer_limit` 추가 5% 보수화 | “전방주시 필요(휴대폰 감지)” + 비프 주기 단축 |
| `drowsy` | WARNING+ | 기본 제한 + `throttle_limit` 추가 10% 보수화 | “졸음 경고, 즉시 주시” + 단계 상승 |
| `unresponsive` | UNRESPONSIVE/ABSENT | Core와 동일(완화 금지) | “반응 없음, 감속/정지 유도” |
| `none/unknown` | 모든 상태 | Core만 적용 | 일반 eyes-on 경고 |

---

## 4) 정책 불변식

- 상위 상태 완화 금지: `ABSENT > UNRESPONSIVE > WARNING > OK`
- 브레이크 미장착 제약: 긴급 대응은 `throttle=0` 중심
- stale 입력 시 overlay 비활성, Core만 유지
- `emergency_flag=ON(hard)`이면 항상 `throttle_limit=0.0`

---

## 5) 구현 인터페이스 권장

- 함수 시그니처 예시:

```text
PolicyOutput evaluate_policy(
  DriverState driver_state,
  Reason reason,
  SpeedBand speed_band,
  CurvatureBand curvature_band,
  float lkas_throttle,
  float lkas_steer,
  bool input_stale
)
```

- 반환값:

```text
{ throttle_limit, steer_limit, hmi_action, emergency_flag }
```
