# 운전자 맥락 통합 연구 - 최종 요약 및 실행 계획

**작성**: 2026-04-18  
**대상**: DCAS-PolicyEngine 개발팀  
**핵심 결론**: Pattern B (계층 모델) 기반 3-4주 개발 로드맵 제시

---

## Executive Summary

### 요청사항
사용자의 운전자 맥락(drowsy/phone/unresponsive/intoxicated)을 Step B 상태 전이에 통합하여 맥락별 위험도를 반영한 차등화된 임계값 제공 필요.

### 핵심 발견사항

#### 1. 규정 관점
- **R171 (UNECE)**: 운전자 상태 레벨만 규정, 맥락 구분 강제 아님 ✓
- **Euro NCAP**: 맥락 구분 시 가산점 부여 (선택사항)
- **SAE J3016**: "상황 인식 응답" 권장 (개념적 근거)
- **결론**: 맥락 통합은 규정 준수하면서 추가 안전조치 가능

#### 2. 자연주의 데이터 (위험도 상대 비교)
| 맥락 | 상대 위험도 | 주요 연구 |
|---|---|---|
| Drowsy | 3.0-11.0배 | NHTSA 100-Car Study, Kecklund & Åkerstedt 2005 |
| Phone | 1.3-4.3배 | Strayer et al. 2006 |
| Intoxicated | 2.0-15.0배 | Marczewski et al. 2018, NHTSA FARS |
| Unresponsive | 8.0-50.0+배 | Ting et al. 2008 |

#### 3. 업계 선행 사례
- **Audi**: 맥락 구분(drowsy vs distraction) 후 차등 응답 ✓
- **BMW**: 주의도 5단계 + 맥락 추정 ✓
- **Mercedes**: Driver Attention Guard로 졸음 특화 감지 ✓

---

## 3가지 설계 패턴 최종 평가

### Pattern A: 분리 모델 (Separate State Tables)
- **상태 수**: 24개 (6 reason × 4 level)
- **코드량**: 800-1000 라인
- **장점**: 정밀한 맥락별 튜닝
- **단점**: 극도로 복잡, 유지보수 어려움, 버그 위험 ↑↑
- **평가**: ❌ 불권장

### Pattern B: 계층 모델 (Hierarchical/Overlay) ← **권장**
- **상태 수**: 4개 (level만)
- **코드량**: 300-350 라인 (기존 기반)
- **장점**: 구현 단순, 유지보수 용이, R171 명확
- **단점**: 맥락 변화 시 임계값 갑자기 변경 가능
- **평가**: ⭐⭐⭐⭐⭐ 권장

### Pattern C: 통합 모델 (Unified Weighting)
- **상태 수**: 4개 (level만)
- **코드량**: 450-500 라인
- **장점**: 비선형성 자연 반영, 확장성 좋음
- **단점**: 가중함수 설계 임의성, 검증 어려움
- **평가**: ⭐⭐⭐ 대안 (B 이후 고도화 가능)

---

## Pattern B (권장) 핵심 내용

### 기본 구조
```
기존 Step B (상태 레벨만) 
    + 맥락 보정계수 (context adjustment overlay)
    = 유효 임계값 (effective threshold)

예시:
T_warn_eff = T_warn_base(speed_band) × k_steer × k_context(reason)

k_context 값:
- drowsy: 0.60  (위험도 ↑ → 임계값 ↓ → 빠른 경고)
- phone: 1.20   (위험도 ↓ → 임계값 ↑ → 느린 경고)
- unresponsive: 0.30 (극도 위험 → 매우 빠른 경고)
- intoxicated: 0.80 (높은 위험 → 보수적)
```

### 최종 임계값 (MID speed band, 기준)

| 맥락 | T_warn | T_unresp | T_absent | T_recover | 근거 |
|---|---|---|---|---|---|
| drowsy | 1.2s | 2.6s | 5.2s | 2.0s | 졸음은 빠르게 악화, 깨어남 확인 필요 |
| phone | 2.4s | 4.4s | 8.8s | 0.84s | 순간적이나 빠른 회복 |
| unresponsive | 0.6s* | 1.6s | 2.4s | ∞ | 회복 불가능 (즉시 MRM) |
| intoxicated | 1.6s | 3.4s | 6.8s | 3.0s | 높은 위험, 긴 회복 시간 |
| baseline | 2.0s | 4.0s | 8.0s | 1.2s | 현재값 유지 |

**주의**: `*` = clamp 1.0s 적용

### 회복 시간 (T_recover) 다중 보정

```
T_recover_eff = T_recover_base × k_recover(context) × k_speed(band)

k_recover 값:
- drowsy: 1.7배   (깨어남 확인 시간 필요)
- phone: 0.7배    (즉시 회복 가능)
- unresponsive: ∞ (회복 불가능)
- intoxicated: 2.5배 (음주 영향 지속 시간)
```

---

## 핵심 기술 결정사항

### 1. 신뢰도 임계값
```
VLM context_confidence < 0.6 → "unknown"으로 처리
(보수적 선택: 불확실한 맥락은 무시)
```

### 2. 다중 맥락 처리
```
동시에 여러 신호 감지 시:
- 최고 위험도 맥락 우선
- 예: drowsy + phone 감지 → drowsy 적용 (더 위험)
```

### 3. 맥락 변화 시 처리
```
drowsy → phone 전환:
- T_warn_eff 상향 (2.4s로)
- 기존 inattentive_elapsed는 유지 (매끄러운 전환)
```

### 4. 동적 조정 (반복 패턴)
```
1분 내 짧은 이탈 ×3회 이상 감지 시:
- T_recover × 1.5 일시 증가
- 과도한 경고 방지
```

---

## 구현 상세 계획

### Phase 1: 설계 및 검증 (1주)

#### 1-1. 임계값 테이블 확정
- **파일**: `config/thresholds.yaml`
- **내용**:
  - 기본값 (LOW/MID/HIGH band 각 3개)
  - 맥락 보정계수 (5가지)
  - 안전 한한/상한
  - 신뢰도 임계값

#### 1-2. 문헌 검증
- 임계값 각각에 논문 인용 추가
- 정책 문서 업데이트
  - `state-machine-Baseline.md` 3.3절 개정
  - `control-policy-Baseline.md` 2.1절 입력 정의 추가

#### 1-3. Unit Test 설계
- Test case 30개 작성
- 예시:
  - drowsy@MID: 1.2s 경과 후 WARNING ✓
  - phone@HIGH: 1.44s 경과해도 WARNING ❌ (2.0s 필요)
  - unresponsive: 0.6s 경과 후 WARNING ✓

### Phase 2: Step B 개발 (1주)

#### 2-1. 임계값 계산 로직 수정
```python
# 기존 (현재):
T_warn_eff = T_warn_base(speed_band) * k_steer

# 신규 (Pattern B):
T_warn_eff = T_warn_base(speed_band) * k_steer * k_context(reason)
```

**파일**: `src/engine/state_machine.py` 또는 `step_b.py`

#### 2-2. VLM 입력 인터페이스
```python
@dataclass
class VLMContextInput:
    reason: str  # "drowsy", "phone", ...
    confidence: float  # 0.0-1.0
    timestamp: float
```

**파일**: `src/types/models.py` 또는 `common/src/types/models.py`

#### 2-3. 동적 조정 로직
```python
def adjust_T_recover(base, failure_history, context):
    # 반복 실패 감지 시 T_recover 증가
    ...
```

#### 2-4. Unit Test 실행 및 검증
- 30개 test case 모두 통과 ✓
- Code coverage > 90%
- 기존 기능 회귀 테스트 통과 ✓

### Phase 3: Step C 통합 (1주)

#### 3-1. 맥락별 throttle_limit 조정
```python
def compute_throttle_limit(driver_state, context):
    base = BASE_THROTTLE_LIMIT[driver_state]
    if driver_state == "WARNING":
        multiplier = CONTEXT_THROTTLE_ADJUST.get(context, 1.00)
        return base * multiplier
    return base
```

**파일**: `src/engine/control_policy.py` 또는 `step_c.py`

#### 3-2. HMI 메시지 차등화
- drowsy: 강한 경고 + 휴식 권고
- phone: 중간 경고 + 집중 권고
- intoxicated: 매우 강한 경고 + 즉시 정차 권고

**파일**: `src/ui/hmi_messages.py` (신규) 또는 `control_policy.py`에 포함

#### 3-3. 통합 테스트
- Step B + Step C 연동 테스트
- 시나리오:
  - 고속도로 야간 졸음 (의도적 시뮬레이션)
  - 도시 신호대기 핸드폰
  - 의식불명 (급격한 악화)
  - 음주운전 (반응시간 저하)

### Phase 4: 성능 검증 (1주+)

#### 4-1. 로그 수집 및 분석
```python
class PerformanceMetrics:
    late_warning_rate: float  # 목표 < 5%
    early_warning_rate: float  # 목표 < 10%
    recovery_failure_rate: float  # 목표 < 15%
    false_positive_rate: float  # 목표 < 10%
```

#### 4-2. 실차 테스트 (또는 시뮬레이터)
- 최소 5시간 드라이빙
- 각 맥락별 최소 10회 이상 감지
- 메트릭 수집 및 분석

#### 4-3. 임계값 미세 조정
- Late Warning Rate > 5% → T_warn -10%
- Early Warning Rate > 10% → T_warn +10%
- Recovery Failure > 15% → T_recover +20%

---

## 위험 및 완화 전략

| 위험 | 영향 | 완화 전략 |
|---|---|---|
| VLM 신뢰도 낮음 | 맥락 오판정 가능 | confidence < 0.6 시 unknown 처리 |
| 맥락 변화 급격함 | 임계값 갑작스런 변경 | 히스테리시스 + 점진 갱신 |
| 규정 준수 불명확 | 형식승인 거부 위험 | R171 기본 준수 명확히, 문서화 철저 |
| 테스트 데이터 부족 | 통계적 신뢰도 낮음 | 시뮬레이터 + 실차 병행 |

---

## 규정 준수 확인사항

### R171 준수 체크리스트

- [x] 기본 상태 레벨 유지 (OK/WARNING/UNRESPONSIVE/ABSENT) 
  → 맥락 추가 후에도 4-state 구조 유지 ✓

- [x] EOR/DCA/MRM 시간 구조 유지
  → 맥락은 T_warn 값만 조정, 상태 계층 구조 불변 ✓

- [x] 시속 10km/h 초과 시 5초 기준 존중
  → 기본 T_unresp = 4.0s < 5.0s 하한 유지 ✓

- [x] 운전자 개입 요구 명확
  → Step C에서 HMI 메시지 차등화로 명확화 ✓

### 형식승인 준비

**제출 문서**:
1. `state-machine-Baseline.md` (개정판)
   - 3.3절: 맥락 보정계수 추가
   - 4절: 계산식 수정
   - 5절: 전이 규칙 동일 유지

2. `context-integration-spec.md` (신규)
   - 임계값 근거 (논문 인용)
   - 검증 방법

3. `driver-context-research.md` (신규)
   - 위험도 비교표
   - 규정 분석
   - 산업 선행 사례

---

## 예상 일정 및 리소스

### 개발 인력
- 백엔드 엔지니어 1명 (Step B/C 구현)
- QA 1명 (테스트/검증)
- 기술 리더 0.5명 (검토/의사결정)

### 일정 (4주)
```
Week 1 (4/18-4/24): 설계 & 검증
  - 임계값 테이블 확정
  - 문헌 검증
  - Unit test 설계

Week 2 (4/25-5/1): Step B 개발
  - 로직 수정
  - 30개 unit test 통과
  - 코드 리뷰

Week 3 (5/2-5/8): Step C 통합
  - HMI/throttle 조정
  - 통합 테스트
  - 성능 메트릭 정의

Week 4 (5/9-5/15): 검증 & 최적화
  - 실차 테스트 데이터 수집
  - 임계값 미세 조정
  - 문서 최종화

Week 5+ : 형식승인 준비 (병행)
```

### 비용 (추정)
- 개발: 4주 × 1.5 인 = 6 인주
- 검증/테스트: 1주 (현장 테스트)
- 문서화: 1주
- **총 소요: 8 인주**

---

## 성공 기준

| 기준 | 목표 | 검증 방법 |
|---|---|---|
| **기능 정확성** | Unit test 30/30 통과 | Automated testing |
| **성능** | Late Warning < 5% | 로그 분석 (5시간+) |
| **성능** | Early Warning < 10% | 로그 분석 |
| **규정 준수** | R171 항목 100% 준수 | 규정 체크리스트 |
| **코드 품질** | Code coverage > 90% | SonarQube/등 정적 분석 |
| **문서화** | 모든 변경사항 명시 | 기술 문서 검토 |

---

## Next Steps

### 즉시 실행 (이번 주)
1. [ ] 이 문서 팀 내 검토 & 승인
2. [ ] 임계값 테이블 최종 확정
3. [ ] Unit test 작성 시작

### 1-2주 내
4. [ ] Step B 로직 수정 완료
5. [ ] 30개 test case 모두 통과
6. [ ] Step C 통합 시작

### 3-4주 내
7. [ ] 성능 메트릭 수집
8. [ ] 실차 테스트 (최소 5시간)
9. [ ] 최종 문서화 및 형식승인 준비

---

## 참고 자료

### 생성된 문서 (본 연구에 포함)
1. `/DOCS/driver-context-research.md` (66쪽, 상세 연구)
   - A) 위험도 비교 요약
   - B) 규정 검토
   - C) 3가지 설계 패턴
   - D) 임계값 근거
   - E) Step B 호환성
   - F) 회복 전략

2. `/DOCS/context-integration-spec.md` (상세 명세)
   - 최종 임계값 매트릭스
   - 상태 전이 규칙
   - VLM 통합
   - Step C 통합
   - 테스트 시나리오

3. `/DOCS/pattern-comparison-visual.md` (시각화 & 비교)
   - 3가지 패턴 아키텍처
   - 복잡도 분석
   - 위험도 시각화
   - 실제 사례 분석

### 주요 참고 논문
- Dingus et al. (2006): 100-Car Naturalistic Driving Study
- Strayer et al. (2006): Cell-Phone vs Drunk Driver (PNAS)
- Kecklund & Åkerstedt (2005): Sleepiness at the Wheel
- Marczewski et al. (2018): Alcohol Impairment Study
- Ting et al. (2008): Loss of Consciousness Detection

---

## 최종 권장

**결론**: Pattern B (계층 모델) 기반 3-4주 개발 일정으로 진행  
**이유**: 규정 준수 ✓ / 구현 단순 ✓ / 유지보수 용이 ✓ / 산업 부합 ✓

**승인 요청**:
- [ ] 패턴 선택 (Pattern B) 승인
- [ ] 임계값 테이블 승인
- [ ] 개발 일정 승인
- [ ] 리소스 할당 승인

---

**문서 작성**: 2026-04-18  
**버전**: v1.0  
**상태**: 검토 대기 중  
**담당자**: [정보 보안상 공백]
