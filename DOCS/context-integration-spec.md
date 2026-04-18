# 운전자 맥락 통합 설계 명세 (Implementation Specification)

**문서 버전**: v1.0  
**대상**: DCAS-PolicyEngine Step B/C 통합 개발  
**기준선**: Pattern B (Hierarchical Context Model)  
**작성일**: 2026-04-18

---

## 1. 상세 임계값 테이블 (최종 권장)

### 1.1 기본 임계값 (속도 Band별)

```yaml
# File: config/thresholds.yaml

thresholds:
  base:
    LOW:
      speed_range: [0, 0.30]
      T_warn: 3.0    # 초
      T_unresp: 6.0
      T_absent: 10.0
      T_recover: 1.0
      k_steer_LOW: 1.00
      k_steer_MID: 0.90
      k_steer_HIGH: 0.80
    
    MID:
      speed_range: [0.30, 0.65]
      T_warn: 2.0
      T_unresp: 4.0
      T_absent: 8.0
      T_recover: 1.2
      k_steer_LOW: 1.00
      k_steer_MID: 0.90
      k_steer_HIGH: 0.80
    
    HIGH:
      speed_range: [0.65, 1.0]
      T_warn: 1.5
      T_unresp: 3.0
      T_absent: 6.0
      T_recover: 1.5
      k_steer_LOW: 1.00
      k_steer_MID: 0.90
      k_steer_HIGH: 0.80

  context_adjustments:
    drowsy:
      k_warn: 0.60
      k_unresp: 0.65
      k_recover: 1.70
      rationale: "PERCLOS 기반 졸음 감지 시 빠른 경고 필요 (Kecklund & Åkerstedt 2005)"
      
    phone:
      k_warn: 1.20
      k_unresp: 1.10
      k_recover: 0.70
      rationale: "순간적 조작, 빠른 회복 (Strayer et al. 2006)"
      
    unresponsive:
      k_warn: 0.30
      k_unresp: 0.40
      k_recover: inf
      rationale: "의식불명 - 회복 불가능, 즉시 MRM (Ting et al. 2008)"
      
    intoxicated:
      k_warn: 0.80
      k_unresp: 0.85
      k_recover: 2.50
      rationale: "높은 위험도, 긴 회복 시간 (Marczewski et al. 2018)"
      
    unknown:
      k_warn: 1.00
      k_unresp: 1.00
      k_recover: 1.00
      rationale: "맥락 불명확 - 기본값 사용"

  # 안전 한한/상한
  safety_limits:
    T_warn_min: 1.0
    T_warn_max: 5.0
    T_unresp_min: 1.5
    T_recover_min: 0.5

  # 신뢰도 임계값
  confidence_thresholds:
    low: 0.6  # 0.6 이하면 context를 unknown으로 처리
```

---

### 1.2 속도 Band별 상세 임계값 계산

#### LOW Band (0-30km/h, rho_v < 0.30)

| 맥락 | T_warn | T_unresp | T_absent | T_recover | k_context |
|---|---|---|---|---|---|
| drowsy | 1.8s | 3.9s | 6.5s | 1.7s | 0.60 |
| phone | 3.6s | 6.6s | 11.0s | 0.7s | 1.20 |
| unresponsive | 0.9s* | 2.4s | 3.0s | ∞ | 0.30 |
| intoxicated | 2.4s | 5.1s | 8.5s | 2.5s | 0.80 |
| **baseline** | 3.0s | 6.0s | 10.0s | 1.0s | 1.00 |

**계산식** (drowsy @ LOW):
```
T_warn = 3.0 (base) × 1.00 (steer_LOW) × 0.60 (context) = 1.8s
```

#### MID Band (30-65km/h, 0.30 ≤ rho_v < 0.65)

| 맥락 | T_warn | T_unresp | T_absent | T_recover | 비고 |
|---|---|---|---|---|---|
| drowsy | 1.2s | 2.6s | 5.2s | 2.0s | 가장 짧은 경고 |
| phone | 2.4s | 4.4s | 8.8s | 0.84s | 빠른 회복 |
| unresponsive | 0.6s* | 1.6s | 2.4s | ∞ | 즉시 위험 |
| intoxicated | 1.6s | 3.4s | 6.8s | 3.0s | 긴 회복 |
| **baseline** | 2.0s | 4.0s | 8.0s | 1.2s | 기준값 |

#### HIGH Band (>65km/h, rho_v ≥ 0.65)

| 맥락 | T_warn | T_unresp | T_absent | T_recover | 비고 |
|---|---|---|---|---|---|
| drowsy | 0.72s* | 1.56s | 3.12s | 2.4s | 매우 짧은 경고 |
| phone | 1.44s | 2.64s | 5.28s | 1.05s | 고속: 약간 보수 |
| unresponsive | 0.36s* | 0.96s | 1.44s | ∞ | 극도 보수적 |
| intoxicated | 0.96s | 2.04s | 4.08s | 3.75s | 매우 긴 회복 |
| **baseline** | 1.2s | 2.4s | 4.8s | 1.5s | 고속 기준 |

**주의**: `*` 표시는 clamp 하한 1.0s 적용 필수

---

## 2. 상태 전이 규칙 (상세)

### 2.1 전이 행렬

```
현재 상태 → 조건 → 다음 상태

OK → inattentive_elapsed >= T_warn_eff → WARNING
WARNING → inattentive_elapsed >= T_unresp_eff → UNRESPONSIVE
UNRESPONSIVE → inattentive_elapsed >= T_absent_eff → ABSENT
WARNING/UNRESPONSIVE/ABSENT → is_attentive=yes && recover_elapsed >= T_recover_eff → 한 단계 하향
ABSENT/UNRESPONSIVE/WARNING → recover_elapsed >= T_clear (3.0s) → OK (완전 복귀)
ANY → input_stale=true → Fail-safe 적용
```

### 2.2 타이머 관리 (상세)

```python
# 상태별 타이머 상태

class TimerState:
    inattentive_elapsed: float = 0.0  # is_attentive=no일 때만 누적
    recover_elapsed: float = 0.0      # is_attentive=yes일 때만 누적
    
    def update_inattentive(self, dt: float, is_attentive: bool):
        """집중 상태 시간 누적"""
        if is_attentive:
            # 복귀 신호 → recover 타이머만 누적
            return
        else:
            # 이탈 신호 → inattentive 누적, recover 리셋
            self.inattentive_elapsed += dt
            self.recover_elapsed = 0.0
    
    def reset_on_full_recovery(self):
        """완전 복귀 시 리셋"""
        self.inattentive_elapsed = 0.0
        self.recover_elapsed = 0.0
```

### 2.3 히스테리시스 & 단계 하향 처리

```python
def handle_downgrade(current_state: str, T_warn_eff: float) -> float:
    """
    한 단계 하향 시 inattentive_elapsed 감소
    (의도: 반복 경보 회피)
    """
    
    Delta_down = max(0.3, 0.1 * T_warn_eff)
    
    new_elapsed = max(0, inattentive_elapsed - Delta_down)
    
    return new_elapsed

# 예시
# UNRESPONSIVE -> WARNING 시:
#   inattentive_elapsed = min(inattentive_elapsed, T_unresp_eff - Delta_down)
# WARNING -> OK 시:
#   inattentive_elapsed = 0 (완전 리셋, 왜냐하면 OK 상태이므로)
```

---

## 3. VLM 맥락 입력 통합

### 3.1 VLM 인터페이스

```python
@dataclass
class VLMContextInput:
    """
    인지팀 VLM 결과 입력
    (WARNING 상태 진입 시에만 요청)
    """
    
    reason: str  # "drowsy", "phone", "unresponsive", "intoxicated", "unknown"
    confidence: float  # 0.0 ~ 1.0
    timestamp: float  # Unix timestamp
    latency_ms: int  # VLM 처리 시간
    
    def is_valid(self) -> bool:
        return 0.0 <= confidence <= 1.0 and reason in [
            "drowsy", "phone", "unresponsive", "intoxicated", "unknown"
        ]

# Step B 입력 인터페이스 (추가 필드)
@dataclass
class StepBInput:
    is_attentive: bool
    speed_band: str
    steer_band: str
    vlm_context: Optional[VLMContextInput] = None  # 추가
    
    def get_effective_context(self) -> str:
        """신뢰도 기반 실제 사용할 맥락 반환"""
        if self.vlm_context is None:
            return "unknown"
        
        if self.vlm_context.confidence < 0.6:
            return "unknown"
        
        return self.vlm_context.reason
```

### 3.2 VLM 호출 타이밍

```
시작 (OK) 
  ↓
is_attentive=no 감지 후 T_warn_eff 경과
  ↓
WARNING 진입 → vlm_request_hint = "request_context"
  ↓
노트북 VLM 호출 (후행 처리, 50-200ms 지연)
  ↓
VLM 결과 반환 (reason, confidence)
  ↓
Step B: reason별 k_context 적용 (T_warn_eff 갱신)
  ↓
Step C: reason 정보로 HMI 강도 조정
```

### 3.3 VLM 신뢰도 낮음 처리

```python
def apply_vlm_context(
    vlm_input: Optional[VLMContextInput],
    speed_band: str,
    steer_band: str,
    current_thresholds: dict
) -> dict:
    """
    VLM 결과를 임계값에 반영
    신뢰도 낮으면 무시 (보수적 처리)
    """
    
    if vlm_input is None or vlm_input.confidence < 0.6:
        # VLM 정보 무시 → baseline 사용
        return current_thresholds
    
    # VLM 정보 적용
    reason = vlm_input.reason
    k_context = CONTEXT_ADJUSTMENTS[reason].k_warn
    
    # T_warn_eff 재계산
    T_warn_base = BASE_THRESHOLDS[speed_band]["T_warn"]
    k_steer = STEER_ADJUSTMENTS[steer_band]
    
    new_thresholds = dict(current_thresholds)
    new_thresholds["T_warn_eff"] = T_warn_base * k_steer * k_context
    new_thresholds["context"] = reason
    
    return new_thresholds
```

---

## 4. Step C (제어 정책) 통합

### 4.1 맥락별 제어 강도

```python
# 기본 정책 (상태만 고려)
BASE_THROTTLE_LIMIT = {
    "OK": 1.00,
    "WARNING": 0.60,
    "UNRESPONSIVE": 0.20,
    "ABSENT": 0.0,
}

# 맥락 보정계수 (WARNING 상태에서만 적용)
CONTEXT_THROTTLE_ADJUST = {
    "drowsy": 0.90,         # 0.60 × 0.90 = 0.54 (더 강한 제한)
    "phone": 1.05,          # 0.60 × 1.05 = 0.63 (약간 약함)
    "unresponsive": 0.70,   # 0.60 × 0.70 = 0.42 (매우 강함)
    "intoxicated": 0.85,    # 0.60 × 0.85 = 0.51 (강함)
    "unknown": 1.00,        # 0.60 (기준)
    "none": 1.00,
}

def compute_throttle_limit(
    driver_state: str,
    context: str
) -> float:
    """맥락을 반영한 최종 제어값"""
    
    base_limit = BASE_THROTTLE_LIMIT[driver_state]
    
    if driver_state == "WARNING":
        multiplier = CONTEXT_THROTTLE_ADJUST.get(context, 1.00)
        return base_limit * multiplier
    else:
        # WARNING 외 상태에서는 맥락 미적용
        return base_limit
```

### 4.2 HMI 메시지 차등화

```python
def get_hmi_message(
    driver_state: str,
    context: str,
    warning_count: int  # 누적 경고 횟수
) -> dict:
    """상태+맥락+이력 기반 HMI 메시지"""
    
    if driver_state == "OK":
        return {"message": "정상 주행", "level": "info"}
    
    elif driver_state == "WARNING":
        if context == "drowsy":
            return {
                "message": "졸음 운전 경고! 휴식이 필요합니다.",
                "sound": "beep_strong",
                "haptic": "vibrate_strong"
            }
        elif context == "phone":
            return {
                "message": "도로를 주시하세요",
                "sound": "beep_soft",
                "haptic": "vibrate_medium"
            }
        elif context == "intoxicated":
            return {
                "message": "즉시 안전한 장소에 정차하세요",
                "sound": "beep_very_strong",
                "haptic": "vibrate_very_strong"
            }
        else:
            return {
                "message": "집중하세요",
                "sound": "beep_medium",
                "haptic": "vibrate_medium"
            }
    
    elif driver_state == "UNRESPONSIVE":
        return {
            "message": "즉시 반응하세요!",
            "sound": "alarm_continuous",
            "haptic": "vibrate_continuous"
        }
    
    elif driver_state == "ABSENT":
        return {
            "message": "운전자 부재 - 안전 정지 중...",
            "sound": "alarm_loudest",
            "haptic": "vibrate_maximum"
        }
```

---

## 5. 테스트 시나리오 (검증용)

### 5.1 기본 시나리오

#### Scenario 1: 정상 주행
```
시간 | 입력 | 예상 상태 | T_warn_eff | 비고
0s   | OK, is_attentive=yes | OK | 2.0s | 기준값
5s   | MID, is_attentive=yes | OK | 2.0s | 유지
```

#### Scenario 2: 졸음 감지 (MID, LOW 조향)
```
시간 | 입력 | 맥락 | 예상 상태 | T_warn_eff | 실제 경과시간
0s   | is_attentive=yes | - | OK | 2.0s | 0s
2.5s | is_attentive=no, vlm_conf=0.8 | drowsy | OK | 1.2s | 2.5s (VLM 지연)
3.7s | is_attentive=no | drowsy | WARNING | 1.2s | 4.2s 총경과
(T_warn_eff = 2.0 × 0.9 × 0.6 = 1.08s, clamp = 1.08s ≈ 1.2s)
```

#### Scenario 3: 핸드폰 사용 (HIGH 속도, MID 조향)
```
시간 | 입력 | 맥락 | 예상 상태 | T_warn_eff | 예상 경과시간
0s   | is_attentive=yes | - | OK | 1.44s | 0s
1.0s | is_attentive=no | - | OK | 1.44s | 1.0s (VLM 아직)
1.5s | vlm 결과 phone, conf=0.9 | phone | OK | 1.73s | 1.5s (임계값 상향)
2.0s | is_attentive=no | phone | OK | 1.73s | 2.0s
3.8s | is_attentive=no | phone | WARNING | 1.73s | 3.8s 총경과
```

#### Scenario 4: 의식불명 (즉시 위험)
```
시간 | 입력 | 맥락 | 예상 상태 | T_warn_eff | 경과시간
0s   | is_attentive=yes | - | OK | 2.0s | 0s
0.5s | is_attentive=no, vlm='unresponsive', conf=0.95 | unresponsive | OK | 0.6s* | 0.5s
1.1s | is_attentive=no | unresponsive | WARNING | 0.6s | 1.1s 총경과
1.6s | is_attentive=no | unresponsive | UNRESPONSIVE | 1.6s | 1.6s
(T_recover_eff = ∞ → 회복 불가능)
```

### 5.2 엣지 케이스

#### Edge Case 1: 맥락 변화 (drowsy → phone)
```
시간 | 상태 | 맥락 | T_warn_eff | 주석
0s   | WARNING | drowsy | 1.2s | 졸음 상태
5s   | WARNING | drowsy | 1.2s | 여전히 졸음
6.0s | WARNING | phone (vlm 재호출) | 2.4s | 맥락 변화 감지
(임계값 상향 → 경보 해제 가능성 ↑)
```

#### Edge Case 2: 낮은 신뢰도 VLM
```
시간 | vlm_reason | vlm_conf | 적용 | T_warn_eff
2.5s | drowsy | 0.55 | ❌ 무시 | 2.0s (baseline)
(신뢰도 < 0.6 → unknown으로 처리, 보수적 선택)
```

#### Edge Case 3: 반복적 짧은 이탈
```
반복 패턴: 0.5s off → 1.0s on → 0.5s off × 5회 (1분)

1st 사이클 (0-1.5s):
  T_recover_eff = 1.2s
  recover_elapsed = 1.0s (< 1.2s) → 복귀 못함, WARNING 유지

Accumulated failure: failure_count += 1
(향후 T_recover × 1.5 적용 가능)
```

---

## 6. 로깅 & 모니터링

### 6.1 필수 로그 항목

```json
{
  "timestamp": 1234567890.123,
  "frame_id": 12345,
  "is_attentive": false,
  "speed_band": "MID",
  "steer_band": "LOW",
  "driver_context": "drowsy",
  "context_confidence": 0.85,
  "thresholds": {
    "T_warn_eff": 1.08,
    "T_unresp_eff": 2.6,
    "T_absent_eff": 5.2,
    "T_recover_eff": 2.0
  },
  "timers": {
    "inattentive_elapsed": 3.5,
    "recover_elapsed": 0.0
  },
  "current_state": "WARNING",
  "next_state": "WARNING",
  "transition_reason": "no_transition",
  "throttle_limit": 0.54,
  "hmi_action": "EOR_drowsy"
}
```

### 6.2 모니터링 메트릭

```python
class MetricsCollector:
    """성능 모니터링"""
    
    def __init__(self, window_size: int = 300):  # 5분 윈도우
        self.window_size = window_size
        self.late_warning_count = 0        # 경고 늦은 횟수
        self.early_warning_count = 0       # 조기 경고
        self.recovery_failure_count = 0    # 회복 실패
        self.false_positive_count = 0      # 오경고
    
    def compute_metrics(self) -> dict:
        total = self.window_size
        
        return {
            "late_warning_rate": self.late_warning_count / total,
            "early_warning_rate": self.early_warning_count / total,
            "recovery_failure_rate": self.recovery_failure_count / total,
            "false_positive_rate": self.false_positive_count / total,
        }
    
    def should_adjust_threshold(self) -> tuple[bool, str]:
        """임계값 조정 필요 여부 판단"""
        metrics = self.compute_metrics()
        
        if metrics["late_warning_rate"] > 0.05:
            return (True, "REDUCE_T_WARN_BY_10%")
        elif metrics["early_warning_rate"] > 0.10:
            return (True, "INCREASE_T_WARN_BY_10%")
        elif metrics["recovery_failure_rate"] > 0.15:
            return (True, "INCREASE_T_RECOVER_BY_20%")
        
        return (False, "NO_ADJUSTMENT_NEEDED")
```

---

## 7. 구현 체크리스트

- [ ] 1단계: 임계값 테이블 정의 (`thresholds.yaml`)
- [ ] 2단계: 상태 전이 로직 수정 (Step B)
  - [ ] VLM 입력 인터페이스 추가
  - [ ] 맥락 보정계수 적용
  - [ ] 회복 시간 동적 조정
- [ ] 3단계: Step C 통합
  - [ ] 맥락별 throttle_limit 조정
  - [ ] HMI 메시지 차등화
- [ ] 4단계: 테스트 (시뮬레이션)
  - [ ] 기본 시나리오 ×4
  - [ ] 엣지 케이스 ×3
  - [ ] 회귀 테스트 (기존 기능)
- [ ] 5단계: 성능 평가 (로그 분석)
  - [ ] Late Warning Rate < 5%
  - [ ] False Positive < 10%
  - [ ] Recovery Failure < 15%
- [ ] 6단계: 문서화 완성
  - [ ] 코드 주석 (각 함수 목적)
  - [ ] 변경사항 명시 (why/what)
  - [ ] 튜닝 가이드 작성

---

**다음 문서**: `step-b-implementation.md` (상세 Python 코드 예시)
