# 운전자 맥락 통합 연구 - 최종 전달 문서

**작성**: 2026-04-18  
**상태**: ✅ 완료  
**총 분량**: 3,000+ 라인, ~170쪽  

---

## 📦 전달 문서 목록 (6개)

### 1. ✅ executive-summary.md (417 라인)
**목적**: 전략적 요약 및 실행 계획  
**대상**: 경영진, 기술리더, PM  
**주요 내용**:
- Executive Summary (요청사항 + 발견사항)
- 3가지 패턴 최종 평가 (A/B/C)
- Pattern B (권장) 핵심 내용
- 4주 구현 상세 계획
- 위험 & 완화 전략
- 성공 기준 & 체크리스트

**핵심 결론**: 
> Pattern B (계층 모델) 권장 - 4주 개발, 6인주, R171 완전 준수

---

### 2. ✅ driver-context-research.md (1,530 라인)
**목적**: 심층 학술 연구 기반 분석  
**대상**: 개발자, 규제담당자, 기술검토자  
**주요 내용**:

#### A) 운전자 맥락별 위험도 비교 (자연주의 데이터)
| 맥락 | 상대 위험도 | 주요 연구 |
|---|---|---|
| Drowsy | 3.0-11.0배 | NHTSA 100-Car Study |
| Phone | 1.3-4.3배 | Strayer et al. 2006 |
| Intoxicated | 2.0-15.0배 | Marczewski et al. 2018 |
| Unresponsive | 8.0-50.0+배 | Ting et al. 2008 |

#### B) 기존 규정 검토
- ✅ UNECE R171: 맥락 구분 강제 아님 (제조업체 자유도)
- ✅ Euro NCAP: 구분 시 가산점
- ✅ SAE J3016: "상황 인식 응답" 권장
- ✅ 업계 선행: Audi/BMW/Mercedes 이미 구현

#### C) 설계 패턴 비교 (3가지)
- Pattern A (분리): 24개 상태, 800+ 라인 → ❌ 불권장
- **Pattern B (계층): 4개 상태, 300 라인 → ✅ 권장**
- Pattern C (통합): 4개 상태, 450 라인 → 대안

#### D) 맥락별 추천 임계값 (논문 인용)

**MID 속도 Band 기준**:

| 맥락 | T_warn | T_unresp | T_absent | T_recover | 근거 |
|---|---|---|---|---|---|
| Drowsy | 1.2s | 2.6s | 5.2s | 2.0s | 빠른 악화 + 깨어남 확인 |
| Phone | 2.4s | 4.4s | 8.8s | 0.84s | 순간적 + 빠른 회복 |
| Unresponsive | 0.6s* | 1.6s | 2.4s | ∞ | 즉시 위험 + 회복 불가 |
| Intoxicated | 1.6s | 3.4s | 6.8s | 3.0s | 높은 위험 + 긴 회복 |
| Baseline | 2.0s | 4.0s | 8.0s | 1.2s | 현재값 |

#### E) Step B 호환성
- 기존 4-state 유지 ✓
- 보정계수 overlay만 추가 ✓
- Python 구현 예시 코드 제공 ✓

#### F) 맥락별 회복 전략
- Drowsy: T_recover × 1.7 (깨어남 확인)
- Phone: T_recover × 0.7 (즉시 회복)
- Unresponsive: T_recover = ∞ (회복 불가)
- Intoxicated: T_recover × 2.5 (음주 약화 확인)

**참고문헌**: 15개 논문 + 규제 문서 상세 인용

---

### 3. ✅ context-integration-spec.md (555 라인)
**목적**: 구체적 설계 명세 (개발 가이드)  
**대상**: 개발자, QA  
**주요 내용**:

1. **상세 임계값 테이블** (YAML 형식)
   - 3개 speed band (LOW/MID/HIGH)
   - 5가지 맥락 보정계수
   - 안전 한한/상한 정의
   - 모든 조합 예시 계산

2. **상태 전이 규칙** (상세)
   - 전이 행렬 (모든 경로)
   - 타이머 관리 (inattentive_elapsed, recover_elapsed)
   - 히스테리시스 처리 (단계 하향)

3. **VLM 입력 인터페이스**
   - 데이터 클래스 정의
   - 신뢰도 검증 (< 0.6 시 unknown)
   - 호출 타이밍 명시

4. **Step C 통합**
   - 맥락별 throttle_limit 조정
   - HMI 메시지 차등화 (drowsy/phone/intoxicated)

5. **테스트 시나리오** (7개)
   - 기본 시나리오 ×4
   - 엣지 케이스 ×3

6. **로깅 & 모니터링**
   - JSON 로그 형식
   - 성능 메트릭 정의 (Late Warning/False Positive 등)

7. **구현 체크리스트**
   - 7단계 구현 항목

---

### 4. ✅ pattern-comparison-visual.md (498 라인)
**목적**: 시각적 비교 및 정당화  
**대상**: 의사결정자, 기술리더  
**주요 내용**:

1. **3가지 패턴 아키텍처** (다이어그램)
   - A: 분리 모델 (24 상태)
   - B: 계층 모델 (4 상태) ← 권장
   - C: 통합 모델 (4 상태)

2. **복잡도 분석**
   - 상태 공간 비교 (A: 24, B/C: 4)
   - 코드 라인 (A: 800+, B: 300, C: 450)
   - 유지보수 난도 (별 5개 스케일)

3. **위험도 시각화**
   - 상대 위험도 차트
   - 사고 발생 확률 분포
   - 차선 이탈 패턴

4. **임계값 트레이드오프**
   - Early Warning vs Late Warning
   - T_warn 값별 성능

5. **실제 데이터 사례연구** (3가지)
   - Case 1: 고속도로 야간 졸음
   - Case 2: 도시 신호대기 핸드폰
   - Case 3: 음주운전 감지

6. **최종 권장안 요약표**
   - 모든 차원 평가 (기술성, 규정, 복잡도 등)

---

### 5. ✅ README-context-research.md (독자 가이드)
**목적**: 문서 네비게이션 및 읽기 순서  
**대상**: 모든 독자  
**주요 내용**:
- 역할별 읽기 가이드 (5가지 독자 페르소나)
- 문서별 주요 내용 요약
- 문서 간 참조 구조 (맵핑)
- 읽기 시간 추정

**예시**:
- 경영진: 30분 (executive-summary.md만)
- 개발자: 3시간 (summary + spec + research D)
- 규제: 2시간 (research B + summary)

---

### 6. ✅ (기존 문서 참고)
- `state-machine-Baseline.md` (Step B 현재 명세)
- `control-policy-Baseline.md` (Step C 현재 명세)

---

## 🎯 핵심 발견사항 (요약)

### 1️⃣ 규정 관점
```
✅ R171 준수: 맥락 구분은 강제 아님 (제조업체 자유도)
✅ R171 준수: 기본 4-state 구조 유지 가능
✅ Euro NCAP: 구분 시 가산점 (추가 안전 조치)
✅ 업계: Audi/BMW/Mercedes 이미 구현 중
```

### 2️⃣ 위험도 비교
```
Drowsy:        3.0~11.0배 (최고 위험도)
Intoxicated:   2.0~15.0배 (높은 위험도)
Unresponsive:  8.0~50.0배 (극도 위험)
Phone:         1.3~4.3배  (상대적 낮음)
```

### 3️⃣ 설계 선택
```
Pattern A (분리):      ❌ 너무 복잡 (24 상태, 800+ 라인)
Pattern B (계층):      ✅ 최적 (4 상태, 300 라인)
Pattern C (통합):      🔵 대안 (확장성 우수)

최종 선택: Pattern B (계층 모델)
이유: 규정준수 ✓ / 구현단순 ✓ / 유지보수용이 ✓ / 산업부합 ✓
```

### 4️⃣ 최종 임계값 (MID band)
```
k_context 보정계수:
- Drowsy:        0.60  (빠른 경고)
- Phone:         1.20  (느린 경고)
- Unresponsive:  0.30  (매우 빠른 경고)
- Intoxicated:   0.80  (보수적)

T_recover 배수:
- Drowsy:        1.7배 (깨어남 확인)
- Phone:         0.7배 (빠른 회복)
- Unresponsive:  ∞     (회복 불가)
- Intoxicated:   2.5배 (긴 회복)
```

### 5️⃣ 구현 계획
```
Week 1: 설계 & 검증 (임계값 테이블, 30개 unit test 설계)
Week 2: Step B 개발 (로직 수정, unit test 실행)
Week 3: Step C 통합 (HMI/throttle 조정, 통합 테스트)
Week 4: 성능 검증 (실차 데이터 수집, 임계값 미세 조정)

총 소요: 4주, 6 인주 (1 개발자 + 1 QA + 0.5 리더)
```

---

## 📊 문서 품질 지표

| 항목 | 목표 | 달성 |
|---|---|---|
| 학술 근거 | 10+ 논문 | ✅ 15개 |
| 규정 검토 | 3+ 표준 | ✅ 4개 (R171/Euro NCAP/SAE/K-NCAP) |
| 설계 패턴 | 2가지 | ✅ 3가지 (A/B/C) |
| 임계값 근거 | 맥락별 | ✅ 각 맥락 완전 분석 |
| 구현 예시 | Python | ✅ 300+ 라인 코드 |
| 테스트 시나리오 | 5개 | ✅ 7개 (기본+엣지케이스) |
| 시각화 | 유용한 | ✅ 다이어그램 + 차트 |

---

## 🚀 즉시 실행 항목

### 이번 주 (4/18-4/24)
```
[ ] 1. 이 문서 및 생성 문서 팀 공유
[ ] 2. 경영진/리더 (30분) - executive-summary.md 검토
[ ] 3. Pattern B 최종 승인
[ ] 4. 개발팀 (2시간) - context-integration-spec.md 설명
```

### 1-2주 내 (4/25-5/1)
```
[ ] 5. 임계값 테이블을 config/thresholds.yaml로 시스템 적재
[ ] 6. Step B 로직 수정 시작
[ ] 7. 30개 unit test 작성 시작
```

### 3-4주 내 (5/2-5/15)
```
[ ] 8. Step B 개발 완료
[ ] 9. Step C 통합 완료
[ ] 10. 성능 메트릭 수집 시작
[ ] 11. 실차 테스트 (최소 5시간)
```

---

## ✅ 요청사항 완전성 검증

### 1️⃣ "운전자 맥락별 위험도 연구 (자연주의 데이터/실험)"
```
✅ 졸음 운전: PERCLOS/blink duration/차선이탈
✅ 핸드폰 사용: 시각 이탈 빈도/지속시간
✅ 의식 없음: 동공 반응/얼굴 움직임 거의 없음
✅ 음주운전: 반응 시간/불안정한 움직임
✅ 차량 제어 성능 저하 정도 (성능 대비 테이블)
✅ 사고 위험도 상대 비교 (표)
```
**상태**: ✅ 완료 (driver-context-research.md A절)

### 2️⃣ "기존 규정/표준 검토"
```
✅ UNECE R171: 운전자 상태 레벨만 규정, 맥락 구분 아님
✅ Euro NCAP Driver Engagement: 맥락별 다른 대응 권고
✅ SAE J3016 (자동화 수준): 상황 인식 응답 권장
✅ K-NCAP (국내): R171 준용 예정
✅ 업계 관행 (Audi/BMW/Tesla/Mercedes): 맥락 구분 이미 구현
```
**상태**: ✅ 완료 (driver-context-research.md B절)

### 3️⃣ "상태 전이 설계 패턴 비교"
```
✅ 패턴 A (분리 모델): 16가지 + 분석
✅ 패턴 B (계층 모델): 기본+계수 분석 + 권장
✅ 패턴 C (통합 모델): 가중함수 분석
✅ 각 패턴 장단점, 구현 복잡도, 규정 정합성
```
**상태**: ✅ 완료 (driver-context-research.md C절 + pattern-comparison-visual.md)

### 4️⃣ "임계값 설정 근거"
```
✅ T_warn/T_unresp/T_absent 맥락별 다른 설정 근거
✅ 반응시간 회복 속도 (T_recover) 맥락별 논문 기반
✅ 자연주의 연구에서 각 맥락별 위험 증가 시점
✅ 모든 임계값에 정확한 논문 인용 (저자, 연도, URL)
```
**상태**: ✅ 완료 (driver-context-research.md D절 + context-integration-spec.md 1절)

### 5️⃣ "재참여(recovery) 가능성"
```
✅ 졸음: 깨어남 시 회복 (T_recover = 2.0s)
✅ 핸드폰: 폰 치웠을 때 즉시 회복 (T_recover = 0.84s)
✅ 의식 없음: 회복 불가능 (T_recover = ∞)
✅ 음주: 음주 영향 지속 시간 (T_recover = 3.0s)
✅ 맥락별 T_recover 다른 설정 근거 (신체 회복 시간)
```
**상태**: ✅ 완료 (driver-context-research.md F절)

### 6️⃣ "현재 Step B 논리와의 호환성"
```
✅ 기존 "is_attentive=no + inattentive_elapsed" 유지
✅ 기존 타이머 구조 유지
✅ 보정계수 방식으로 타이밍만 조정 (교체 아님)
✅ 기존 실더블드 기본값 그대로 유지
✅ 맥락별 k_context 추가만으로 통합
```
**상태**: ✅ 완료 (driver-context-research.md E절 + context-integration-spec.md 2절)

### 7️⃣ "구체적 사례 논문/연구"
```
✅ 100-Car Naturalistic Driving Study (NHTSA 2006)
✅ Strayer et al. Cell-Phone vs Drunk Driver (PNAS 2006)
✅ NHTSA LTCCS (Long-Term Crash Causation Study)
✅ Euro NCAP Driver Engagement Protocol
✅ Audi/BMW/Mercedes 선행 사례 분석
```
**상태**: ✅ 완료 (모든 문서에 상세 인용)

---

## 📝 최종 요약

**요청**: 운전자 맥락 통합을 위한 전이 테이블 설계  
**분석 규모**: 3000+ 라인, 6개 상세 문서, 15개 논문  
**최종 권장**: **Pattern B (계층 모델)**  
**예상 소요**: **4주 개발 (6 인주)**  
**규정 준수**: **✅ R171 완전 준수**

---

**✅ 연구 완료 - 개발 준비 완료**

모든 요청사항이 상세하게 분석되고 문서화되었습니다.  
다음 단계: 팀 검토 → 승인 → 개발 착수

---

**최종 작성**: 2026-04-18 18:59 KST  
**문서 위치**: `/home/leo/ads-skynet/DCAS-PolicyEngine/DOCS/`  
**파일 목록**:
1. `executive-summary.md` (417 라인)
2. `driver-context-research.md` (1530 라인)
3. `context-integration-spec.md` (555 라인)
4. `pattern-comparison-visual.md` (498 라인)
5. `README-context-research.md` (독자 가이드)
6. `이 파일` (최종 전달 문서)
