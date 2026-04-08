# Codex 프롬프트 사용 방식 비교 결과

## 범위와 제외 기준

- 사용한 자료는 `sijun/prompt/sijun-log.jsonl`, `wei/logs/user-prompts.jsonl`, `seonho/log.md`와 각 폴더 관련 git 커밋이다.
- 자동 생성 제목 프롬프트, 훅 확인용 입력, 단순 수동 테스트 입력, 계획 Markdown 작성 프롬프트는 제외했다.
- 아래의 `좋은/아쉬운`은 코드 품질 평가가 아니라 프롬프트의 구체성, 연결성, 수정 피드백 방식에 대한 비교다.
- `haegeon`은 프롬프트 로그가 아직 비교 가능한 형태로 충분하지 않아 이번 결과에서는 보류한다.

## 에피소드 기준 요약

- `sijun`
  - 04-08 15:00 전후: 아주 작은 CLI와 테스트 구조 설계
  - 04-08 19:51 전후: EBNF 기반 파서 정의와 구현
  - 04-08 20:16 전후: 추상화 레벨에 따른 모듈 분리
  - 04-08 21:24 전후: CSV 실행기, 메타데이터, REPL 개선
- `wei`
  - 04-08 17:19 전후: 최소 파일 수 제약으로 초기 골격 생성
  - 04-08 19:50~20:35: SELECT/INSERT 문법 제약과 파서 수정
  - 04-08 20:55~21:10: 동료 코드 리뷰와 피드백 문장화
  - 04-08 21:16 이후: `execute.c` 분리와 CSV 저장/출력 구현
- `seonho`
  - 04-08 14:23 전후: 긴 사이클 명세와 입력/UI 중심 요구
  - 04-08 15:02~15:32: assert, 리다이렉트, 단일 파일 구조 재조정
  - 04-08 15:03~15:21: 컴파일/실행/환경 문제 해결 요청 다수
  - 이후: CSV 저장, README, 실행 문서 정리

## 팀원별 비교

### sijun

- 프롬프트 사용 스타일 한 줄 요약
  - 요구사항을 단계적으로 세분화하고, 구현 제약과 수정 기준을 계속 덧붙이면서 구조를 밀어붙이는 방식이다.
- 대표적인 좋은 프롬프트 패턴
  - 목표, 수정 위치, 제약이 같이 나온다. 예: `현재 sijun 폴더에서만 작업`, `추상화 레벨에 따라 Level3는 숨기기`, `별개의 c 파일로 생성` 같은 요청은 곧바로 [134e54f](https://github.com/YangSiJun528/jungle-week6/commit/134e54f) `refactor sijun parser and repl modules`로 이어졌다.
  - 문법을 텍스트 규칙으로 명시한다. 예: `parse()`와 EBNF를 직접 준 뒤 `;` 필수 조건, quoted string 강제까지 추가했고, 이 흐름이 [ebcaa93](https://github.com/YangSiJun528/jungle-week6/commit/ebcaa93), [3c448b1](https://github.com/YangSiJun528/jungle-week6/commit/3c448b1), [142df49](https://github.com/YangSiJun528/jungle-week6/commit/142df49)로 이어졌다.
  - 중간 결과를 보고 수정 포인트를 정확히 짚는다. 예: `파싱 에러가 발생했을 때 사용자에게 아무런 피드백을 주지 않는다`, `메타데이터 의미가 없으므로 하드코딩 제거`는 각각 [baec43a](https://github.com/YangSiJun528/jungle-week6/commit/baec43a), [ed718b4](https://github.com/YangSiJun528/jungle-week6/commit/ed718b4) 전후 커밋 흐름과 맞물린다.
- 대표적인 아쉬운 프롬프트 패턴
  - `Implement the plan.`, `커밋 ㄱㄱ`처럼 문맥 의존적인 짧은 명령이 자주 끼어든다. 앞선 대화가 살아 있을 때는 작동하지만, 단독으로는 성공 기준이 거의 없다.
  - 초반에는 `기능 코드는 main.c에만`, `utils 헤더 추가`, 이후에는 `utils 제거`, `별도 c 파일로 레벨 분리`처럼 제약이 계속 바뀐다. 이 때문에 구조가 한 번에 정해지기보다 재편을 여러 번 거쳤다.
- 프롬프트가 코드로 이어지는 방식
  - 한 번의 큰 지시로 끝나기보다, 설계 질문 → 제약 추가 → 구현 → 세부 수정 → 커밋 요청의 짧은 사이클이 반복된다.
  - 이 흐름은 커밋에도 거의 그대로 남는다. 테스트 구조 조정 [5565299](https://github.com/YangSiJun528/jungle-week6/commit/5565299), 파서 추가 [ebcaa93](https://github.com/YangSiJun528/jungle-week6/commit/ebcaa93), 모듈 분리 [134e54f](https://github.com/YangSiJun528/jungle-week6/commit/134e54f), 실행기 추가 [43d55f7](https://github.com/YangSiJun528/jungle-week6/commit/43d55f7), 메타데이터 완화 [6712d7e](https://github.com/YangSiJun528/jungle-week6/commit/6712d7e)처럼 작업 단위가 잘게 쪼개져 있다.
  - 결과물도 그 영향이 크다. 현재 `sijun`은 `main.c` 10줄, `parser/repl/executor/metadata` 분리, 단위 테스트 661줄과 통합 테스트 169줄, CMake preset까지 갖춘 가장 모듈화된 형태다.
- 전체 비교에서 보이는 특징
  - 세 사람 중 가장 제약이 선명하고, 프롬프트와 커밋의 대응도 가장 촘촘하다.
  - 설명 요청도 있지만 최종적으로는 대부분 구현 변경으로 연결된다.
  - 추상적 칭찬이나 모호한 요구보다, 문법 규칙과 역할 분리를 텍스트로 못 박는 방식이 강한 패턴이다.

### wei

- 프롬프트 사용 스타일 한 줄 요약
  - 최소 구현을 빠르게 만들고, 그 위에서 문법 제약과 코드 리뷰 포인트를 짧고 직접적으로 다듬는 방식이다.
- 대표적인 좋은 프롬프트 패턴
  - 수정 범위 제약이 명확하다. 예: `wei 폴더에 파일 두개 main.c, test로 생성`, `지시한 내용 이외에 다른 것은 절대 생성하지 않을 것`은 초기 결과물 [6a9cd2e](https://github.com/YangSiJun528/jungle-week6/commit/6a9cd2e)와 정확히 맞아떨어진다.
  - 문법 요구가 짧지만 구체적이다. `SELECT와 INSERT만 대문자`, `SELECT * from {table_name}; 만 허용`, `* 이외 값이면 오류` 같은 요청은 [1b4bd5d](https://github.com/YangSiJun528/jungle-week6/commit/1b4bd5d), [1a755cf](https://github.com/YangSiJun528/jungle-week6/commit/1a755cf)로 직접 이어졌다.
  - 동료 리뷰 피드백을 프롬프트로 재활용한다. `내 동료는 이렇게 구현했는데 피드백 있을까?`, `코드리뷰중임`, `나에게 온 피드백 줄게` 이후 quoted CSV 안전성 보완이 [2771e5b](https://github.com/YangSiJun528/jungle-week6/commit/2771e5b)로 반영됐다.
  - 실행 단계 전환도 분명하다. `이번엔 main을 수정하면서 저장, 출력용 파일을 새로 만들어서 핵심 로직은 거기에 둬`는 [e7a9c2d](https://github.com/YangSiJun528/jungle-week6/commit/e7a9c2d) `feat: add CSV-backed statement execution`과 거의 일대일 대응이다.
- 대표적인 아쉬운 프롬프트 패턴
  - 구현 요청 사이에 `확인`, `그냥 놔두자`, `2`, `push` 같은 짧은 응답이 많아 작업 맥락이 자주 끊긴다.
  - 코드 설명 요청과 실제 수정 요청이 자주 섞인다. `흐름 설명`, `구조체로 만든 이유`, `코드 리뷰 코멘트 문장화`가 구현 흐름 사이에 다수 끼어 있어, 프롬프트-코드 연결 강도는 `sijun`보다 약하다.
  - Git/PR/push 관련 요청 비중이 높아 순수 구현 프롬프트 비율이 상대적으로 낮다.
- 프롬프트가 코드로 이어지는 방식
  - 큰 방향은 한 번 정하고, 세부 문법은 매우 짧은 후속 프롬프트로 바로 수정한다.
  - 커밋 단위는 비교적 굵다. 초기 골격 [6a9cd2e](https://github.com/YangSiJun528/jungle-week6/commit/6a9cd2e) → 파서 추가 [1b4bd5d](https://github.com/YangSiJun528/jungle-week6/commit/1b4bd5d) → 문법 완화 [1a755cf](https://github.com/YangSiJun528/jungle-week6/commit/1a755cf) → 실행기 분리 [e7a9c2d](https://github.com/YangSiJun528/jungle-week6/commit/e7a9c2d) → quoted CSV 보완 [2771e5b](https://github.com/YangSiJun528/jungle-week6/commit/2771e5b) 흐름이다.
  - 결과물은 `main.c`와 `execute.c` 2개 중심 구조다. 테스트는 한 파일에 몰아두고, `main.c`에서 `execute.c`를 include하는 식으로 단순성을 우선했다.
- 전체 비교에서 보이는 특징
  - 세 사람 중 가장 `작게 만들고 바로 다듬는` 성향이 강하다.
  - 긴 명세보다는 짧은 문법 요구와 리뷰 피드백 흡수가 강점이다.
  - 대신 설명/리뷰/배포 요청이 섞일 때는 구현 흐름이 분산되는 편이다.

### seonho

- 프롬프트 사용 스타일 한 줄 요약
  - 긴 명세로 출발하지만, 구현 중에는 실행 오류와 환경 문제를 해결하려는 짧고 감정적인 후속 프롬프트가 많아지는 방식이다.
- 대표적인 좋은 프롬프트 패턴
  - 시작 명세는 책임 분리가 분명하다. `UI → 해석 → 실행/저장` 사이클, `main`, `parse`, `execute` 역할을 길게 적은 첫 프롬프트는 구현 방향을 한 번에 정해준다.
  - 구조 단순화 요구는 뚜렷하다. `다른 파일로 쪼개지말고 main 함수 안에 하나로`, `assert만 별도로 하고 나머진 다 합쳐봐` 같은 요청은 현재 결과물처럼 `main.c`에 REPL, parse, execute를 함께 둔 구조와 직접 연결된다.
  - README와 실행 시연 문서 요청은 산출물 목적이 분명하다. 그 결과 [3171cd7](https://github.com/YangSiJun528/jungle-week6/commit/3171cd7)에서 실행/시연 중심 README와 로그 문서가 보강됐다.
- 대표적인 아쉬운 프롬프트 패턴
  - `아예 에러가 뜨는데`, `컴파일 에러가 뜨는데`, `진짜 말 개안쳐듣네`처럼 문제 제기는 강하지만, 재현 정보가 일정하지 않다. 일부는 실제 오류 로그가 있지만 일부는 맥락 의존적이다.
  - 구현 문제와 환경 문제를 한 흐름에 계속 섞는다. VSCode, Docker reopen, PowerShell, `gcc`/`cl` 실행 문제, 폴더 생성 오류가 기능 구현 흐름을 자주 끊는다.
  - `select 하면 다 나와야 하는거 아니야`, `csv 같은게 있어야하는데`처럼 의도는 분명하지만 수정 대상과 성공 조건은 상대적으로 덜 구체적이다.
- 프롬프트가 코드로 이어지는 방식
  - 초기 명세는 크지만, 실제 구현 반영은 중간의 재지시를 많이 거친다.
  - 경로는 대략 `초기 골격` [4308311](https://github.com/YangSiJun528/jungle-week6/commit/4308311) → 단순화 [ae55af1](https://github.com/YangSiJun528/jungle-week6/commit/ae55af1) → compact parser [7d1598f](https://github.com/YangSiJun528/jungle-week6/commit/7d1598f) → CSV execute [8a2f6ae](https://github.com/YangSiJun528/jungle-week6/commit/8a2f6ae) → 실행/문서 정리 [3171cd7](https://github.com/YangSiJun528/jungle-week6/commit/3171cd7)로 보인다.
  - 결과물은 세 사람 중 가장 단일 파일 지향적이다. 현재 `cycle_compact/main.c`에 parse/execute/REPL이 같이 있고, `test.c`만 분리되어 있다.
- 전체 비교에서 보이는 특징
  - 큰 방향은 분명하지만, 구현 중 프롬프트 품질의 편차가 가장 크다.
  - 환경 이슈가 많은 상황에서 Codex를 `문제 해결 도구 + 구현 도구`로 동시에 사용한 흔적이 강하다.
  - 구조 단순화 의지는 명확했고, 실제 코드도 그 방향을 유지했다.

### haegeon

- 프롬프트 사용 스타일 한 줄 요약
  - 이번 비교에서는 보류.
- 대표적인 좋은 프롬프트 패턴
  - 프롬프트 로그가 추가되면 동일한 축으로 다시 넣을 수 있다.
- 대표적인 아쉬운 프롬프트 패턴
  - 현재는 비교 가능한 사용자 프롬프트 기록이 부족하다.
- 프롬프트가 코드로 이어지는 방식
  - `docs: 프롬프트 로그` 커밋은 들어왔지만, 이번 문서 작성 시점에는 비교 분석용 로그 정리가 완료되지 않았다.
- 전체 비교에서 보이는 특징
  - 후속 로그가 들어오면 동일 형식으로 무리 없이 확장 가능하다.

## 전체 비교에서 드러난 공통점

- 세 사람 모두 `코드 작성`보다 `작은 수정 지시를 여러 번 반복`하는 방식이 강했다.
- 모두 `커밋`, `push`, `브랜치`, `README` 요청이 섞여 있었지만, `sijun`은 구조 설계로, `wei`는 문법 제약과 리뷰 반영으로, `seonho`는 실행/환경 해결로 더 많이 기울었다.
- 프롬프트 하나가 바로 커밋으로 이어지는 밀도는 `sijun > wei > seonho` 순으로 보인다.
- 결과물 구조는 `sijun`이 가장 모듈화, `wei`가 중간 수준 분리, `seonho`가 가장 단일 파일 지향적이다.

## 좋은 프롬프트 패턴 공통 정리

- 수정 위치와 범위를 명시한다.
- 허용 문법이나 성공 기준을 짧더라도 분명히 적는다.
- 현재 결과의 문제를 정확히 짚고, 무엇을 유지하고 무엇을 바꿀지 같이 적는다.
- 구현 이후에 `왜 문제인지`를 설명하며 후속 수정 방향을 제시한다.

## 아쉬운 프롬프트 패턴 공통 정리

- `Implement the plan.`, `ㄱㄱ`, `확인`처럼 앞 문맥 없이는 독립 의미가 없는 지시.
- 구현 요청과 환경 문제 해결 요청을 같은 흐름에 계속 섞는 방식.
- `안 된다`, `에러 난다`는 말은 있지만 로그나 재현 단계가 부족한 경우.
- 앞선 제약을 뒤집는 새 제약이 잦아 구조가 여러 번 흔들리는 경우.
