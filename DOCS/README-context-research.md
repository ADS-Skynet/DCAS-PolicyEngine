# 운전자 맥락 통합 연구 - 문서 인덱스 및 읽기 가이드

**작성**: 2026-04-18  
**목적**: 생성된 6개 문서의 구조와 읽기 순서 안내  
**대상**: DCAS-PolicyEngine 개발팀, 규제 담당자, 기술 리더

---

## 📋 생성된 문서 목록

| # | 파일명 | 페이지 | 목적 | 대상 |
|---|---|---|---|---|
| 1 | `executive-summary.md` | ~20쪽 | 전략적 요약 & 실행 계획 | 경영진, 기술리더 |
| 2 | `driver-context-research.md` | ~80쪽 | 심층 연구 (학술 기반) | 개발자, 규제담당자 |
| 3 | `context-integration-spec.md` | ~40쪽 | 구체적 설계 명세 | 개발자, QA |
| 4 | `pattern-comparison-visual.md` | ~30쪽 | 시각화 & 비교 분석 | 의사결정자, 개발자 |
| 5 | `state-machine-Baseline.md` | (기존) | 현재 Step B 명세 | 기본 참고 |
| 6 | `control-policy-Baseline.md` | (기존) | 현재 Step C 명세 | 기본 참고 |

---

## 🎯 독자별 읽기 가이드

### 👔 경영진 / 의사결정자

**목표**: 무엇을 왜 하는지, 언제 끝나는지 파악

**읽기 순서** (30분):
1. **`executive-summary.md` (전체)**
   - Executive Summary (요청사항 + 발견사항)
   - 3가지 패턴 최종 평가
   - Pattern B 권장 근거
   - 구현 상세 계획 (단계별)
   - 예상 일정 & 리소스

2. **`pattern-comparison-visual.md`** (섹션 6만)
   - "최종 권장안 요약표"
   - 의사결정 플로우

**결론**: 4주 개발, 6인주, Pattern B 선택 ✓

---

### 👨‍💼 기술 리더 / PM

**목표**: 기술적 타당성 검증 + 리스크 관리

**읽기 순서** (1-2시간):
1. **`executive-summary.md`** (전체)
   - 왜 이 선택인지 이해

2. **`driver-context-research.md`**
   - A) 위험도 비교 (자연주의 데이터)
   - B) 규정 검토 (R171/Euro NCAP/SAE)
   - C) 3가지 패턴 분석 (패턴 B 권장)
   - 결론 및 권장사항

3. **`pattern-comparison-visual.md`** (전체)
   - 아키텍처 시각화
   - 복잡도 분석
   - 실제 사례 연구

4. **`context-integration-spec.md`** (섹션 1-2만)
   - 임계값 테이블
   - 상태 전이 규칙

**확인 체크리스트**:
- [ ] R171 준수 확인 ✓
- [ ] 업계 선행 사례 검토 ✓
- [ ] 예상 일정 타당성 ✓
- [ ] 리스크 완화 전략 ✓

---

### 👨‍💻 개발자 (Step B 담당)

**목표**: 구현할 내용 정확히 이해

**읽기 순서** (2-3시간):
1. **`executive-summary.md`** (섹션 2 ~ 4)
   - 3가지 패턴
   - Pattern B 구조

2. **`driver-context-research.md`** (섹션 D)
   - 각 맥락별 임계값
   - 근거 논문

3. **`context-integration-spec.md`** (전체)
   - 임계값 테이블 (1.1-1.2)
   - 상태 전이 (2절)
   - VLM 입력 (3절)
   - Python 구현 예시
   - 로깅 & 모니터링 (6절)

4. **`state-machine-Baseline.md`** (기존)
   - 현재 Step B 로직 상기
   - 변경 지점 비교

**구현 항목**:
- [ ] `thresholds.yaml` 생성
- [ ] `StateMachineV2.compute_thresholds()` 구현
- [ ] `VLMContextInput` 데이터 클래스 정의
- [ ] 회복 시간 동적 조정 로직
- [ ] 30개 unit test 작성 & 통과

---

### 👩‍🔬 QA / 테스터

**목표**: 검증 시나리오 & 메트릭 정의

**읽기 순서** (1-2시간):
1. **`context-integration-spec.md`**
   - 섹션 5: 테스트 시나리오
   - 섹션 6: 로깅 & 모니터링

2. **`executive-summary.md`**
   - 성공 기준 (테이블)
   - 성능 메트릭 정의

3. **`pattern-comparison-visual.md`** (섹션 5)
   - Early Warning vs Late Warning 트레이드오프

**테스트 준비 항목**:
- [ ] 기본 시나리오 ×4 (정상/졸음/핸드폰/의식불명)
- [ ] 엣지 케이스 ×3 (맥락 변화/신뢰도 낮음/반복)
- [ ] 회귀 테스트 (기존 기능)
- [ ] 성능 메트릭 (Late Warning < 5% 등)
- [ ] 로그 분석 스크립트

---

### 📋 규제 담당자 (형식승인 준비)

**목표**: R171 준수 확인 & 제출 문서 준비

**읽기 순서** (2시간):
1. **`executive-summary.md`**
   - 규정 준수 확인사항
   - 형식승인 준비

2. **`driver-context-research.md`** (섹션 B)
   - R171 분석
   - Euro NCAP
   - SAE J3016
   - K-NCAP

3. **`pattern-comparison-visual.md`** (섹션 2)
   - 규정 검증 열

4. **업계 선행 사례 (참고)**
   - Audi Driver State Classification
   - BMW Attention Assistant
   - Mercedes Driver Attention Guard

**준비 항목**:
- [ ] R171 5.5.4 섹션 완전 준수 확인
- [ ] 상태 레벨 4개 유지
- [ ] 임계값 근거 명시 (논문 인용)
- [ ] 형식승인 신청서 작성
- [ ] 검증 리포트 준비

---

## 🗂️ 문서별 주요 내용

### 1. Executive Summary (~20쪽)

```
구성:
├─ Executive Summary (요청사항 + 발견사항)
├─ 3가지 설계 패턴 최종 평가
├─ Pattern B (권장) 핵심 내용
├─ 기술 결정사항
├─ 구현 상세 계획 (4주 단계별)
├─ 위험 & 완화 전략
├─ 규정 준수 체크리스트
├─ 예상 일정 & 리소스
├─ 성공 기준
└─ Next Steps

목적: 전략적 개요 + 실행 계획 제공
시간: 30분 (빠른 읽기)
```

**주요 테이크아웃**:
- Pattern B = 최고 ROI (복잡도 vs 효과)
- 4주 개발 일정 (6인주)
- R171 완전 준수 ✓

---

### 2. Driver Context Research (80쪽)

```
구성:
├─ A) 위험도 비교 요약 (자연주의 데이터)
│  ├─ 위험도 상대 테이블
│  ├─ 차량 제어 성능 저하
│  └─ 사고 위험도 상대 비교
│
├─ B) 기존 규정 검토
│  ├─ UNECE R171
│  ├─ Euro NCAP
│  ├─ SAE J3016
│  ├─ K-NCAP
│  └─ 업계 관행 (Audi/BMW/Tesla/Mercedes)
│
├─ C) 설계 패턴 분석 (분리/계층/통합)
│  ├─ 아키텍처
│  ├─ 장단점
│  ├─ 복잡도
│  └─ 종합 비교
│
├─ D) 임계값 설정 근거 (논문 기반)
│  ├─ Drowsy (Kecklund & Åkerstedt 2005)
│  ├─ Phone (Strayer et al. 2006)
│  ├─ Unresponsive (Ting et al. 2008)
│  ├─ Intoxicated (Marczewski et al. 2018)
│  └─ 종합 임계값 매트릭스
│
├─ E) Step B 호환성
│  ├─ 기존 로직 분석
│  ├─ Pattern B 통합 방식
│  ├─ Python 구현 예시
│  └─ Step C 통합
│
├─ F) 회복 전략
│  ├─ 맥락별 회복 메커니즘
│  ├─ T_recover 동적 조정
│  ├─ 회복 실패 처리
│  └─ 최종 회복 전략표
│
└─ 참고 문헌 (15개 논문/표준)

목적: 학술 근거 제시
시간: 2-3시간 (심층 읽기)
```

**주요 테이크아웃**:
- Drowsy: 위험도 가장 높음 (3.0-11.0배) → T_warn 짧게
- Phone: 상대적으로 낮음 (1.3-4.3배) → T_warn 길게
- 회복 시간: drowsy(×1.7), phone(×0.7), intoxicated(×2.5)

---

### 3. Context Integration Spec (40쪽)

```
구성:
├─ 상세 임계값 테이블 (yaml 형식)
│  ├─ 기본값 (LOW/MID/HIGH)
│  ├─ 맥락 보정계수
│  ├─ 안전 한한/상한
│  └─ Speed/Steer band 예시
│
├─ 상태 전이 규칙 (상세)
│  ├─ 전이 행렬
│  ├─ 타이머 관리
│  └─ 히스테리시스 처리
│
├─ VLM 맥락 입력 (데이터 클래스)
│  ├─ 인터페이스 정의
│  ├─ 호출 타이밍
│  └─ 신뢰도 처리
│
├─ Step C 통합
│  ├─ 맥락별 throttle_limit
│  └─ HMI 메시지 차등화
│
├─ 테스트 시나리오
│  ├─ 기본 ×4 (정상/졸음/핸드폰/의식불명)
│  └─ 엣지 케이스 ×3
│
├─ 로깅 & 모니터링
│  ├─ 필수 로그 JSON 형식
│  └─ 성능 메트릭 정의
│
└─ 구현 체크리스트

목적: 개발 구현 가이드
시간: 1-2시간 (개발 준비)
```

**주요 테이크아웃**:
- MID band: T_warn_base = 2.0s
- drowsy: k_context = 0.60 → T_warn = 1.2s
- phone: k_context = 1.20 → T_warn = 2.4s
- 30개 test case 필수

---

### 4. Pattern Comparison Visual (30쪽)

```
구성:
├─ 3가지 패턴 아키텍처 (다이어그램)
│  ├─ A: 분리 모델
│  ├─ B: 계층 모델 (권장)
│  └─ C: 통합 모델
│
├─ 상태 공간 & 복잡도 비교
│  ├─ 상태 수
│  ├─ 코드 라인
│  └─ 유지보수 난도
│
├─ 위험도 시각화 (차트)
│  ├─ 상대 위험도
│  ├─ 사고 발생 확률
│  └─ 차선 이탈 패턴
│
├─ 임계값 트레이드오프
│  ├─ Early vs Late Warning
│  └─ 속도 band별 권장값
│
├─ 실제 데이터 기반 사례연구
│  ├─ Case 1: 고속도로 야간 (졸음)
│  ├─ Case 2: 도시 신호 (핸드폰)
│  └─ Case 3: 음주운전 감지
│
└─ 최종 권장안 & 의사결정 플로우

목적: 시각적 비교 & 정당화
시간: 1시간 (의사결정 회의용)
```

**주요 테이크아웃**:
- Pattern B: 4개 상태 유지, 300줄 코드
- 규정 준수 명확, 유지보수 용이
- 3가지 선택 이유: 복잡도 vs 효과 트레이드오프

---

## 🔍 문서 간 참조 구조

```
executive-summary.md
    ↑
    ├── 참조: pattern-comparison-visual.md (섹션 6)
    ├── 참조: context-integration-spec.md (섹션 4)
    └── 참조: driver-context-research.md (섹션 B)

driver-context-research.md (심층)
    ↑
    ├── 인용: 15개 논문
    ├── 참조: state-machine-Baseline.md (기존 Step B)
    ├── 상세: context-integration-spec.md (섹션 D)
    └── 실제: pattern-comparison-visual.md (섹션 5)

context-integration-spec.md (구현)
    ↑
    ├── 출처: driver-context-research.md (임계값)
    ├── 기반: state-machine-Baseline.md
    ├── 통합: control-policy-Baseline.md
    └── 검증: 30개 test case 참고
```

---

## 📊 읽기 시간 추정

| 역할 | 총 시간 | 세부 |
|---|---|---|
| **경영진** | 30분 | Executive Summary 전체 |
| **기술리더** | 2시간 | Summary + Research + Visual |
| **개발자** | 3시간 | Summary + Spec + Research(D섹션) |
| **QA** | 1.5시간 | Spec + Summary |
| **규제** | 2시간 | Research(B) + Summary |
| **전체 이해** | 8시간 | 모든 문서 정독 |

---

## ✅ 문서 준비 완료 체크리스트

- [x] **A) 운전자 맥락별 위험도 비교** (위험도 테이블, 참고 논문)
- [x] **B) 기존 규정/표준 검토** (R171, Euro NCAP, SAE J3016, K-NCAP)
- [x] **C) 상태 전이 설계 패턴 비교** (패턴 A/B/C 상세 분석)
- [x] **D) 각 맥락별 추천 임계값** (MID band 기준, 모든 speed band 예시)
- [x] **E) Step B 호환성** (Python 구현 코드 예시)
- [x] **F) 맥락별 회복 전략** (T_recover 동적 조정, 메커니즘)

---

## 🚀 다음 단계

### 즉시 (이번 주)
1. [ ] 이 문서 및 생성된 문서 팀 공유
2. [ ] 경영진/리더 검토 및 승인 (executive-summary.md)
3. [ ] Pattern B 선택 최종 확정

### 1-2주 내
4. [ ] 개발팀 상세 설명 (context-integration-spec.md)
5. [ ] 임계값 테이블 시스템 적재
6. [ ] Unit test 작성 시작

### 3-4주 내
7. [ ] Step B 로직 구현 완료
8. [ ] Step C 통합 완료
9. [ ] 성능 메트릭 수집 및 분석

---

## 📞 문의 & 피드백

**문서별 담당자 (예시)**:
- Executive Summary: 기술리더
- Research: 개발팀 리드
- Spec: 개발자
- Visual: PM/의사결정자
- 규정 검토: 규제담당자

**피드백 항목**:
- 임계값 타당성 검토?
- 구현 일정 타당성?
- 테스트 시나리오 충분?
- 규정 준수 완전성?

---

**문서 작성 완료**: 2026-04-18  
**총 분량**: ~170쪽 (6개 문서)  
**상태**: 검토 대기 중  
**배포**: DCAS-PolicyEngine/DOCS/ 폴더
