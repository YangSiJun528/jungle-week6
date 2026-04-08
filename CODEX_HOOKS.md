# Codex Hooks

이 저장소는 Codex의 `UserPromptSubmit` 훅을 사용합니다.

## 무엇을 하나요?

사용자가 프롬프트를 보낼 때마다 루트 `.codex/hooks/user_prompt_submit_log.py`가 실행되어 입력 내용을 JSONL 형식으로 기록합니다.

## 어떻게 설정되어 있나요?

- `.codex/config.toml`: `codex_hooks = true`로 훅 기능 활성화
- `.codex/hooks.json`: `UserPromptSubmit` 이벤트에 Python 스크립트 연결
- `.codex/hooks/user_prompt_submit_log.py`: 표준 입력으로 받은 훅 payload를 로그 파일에 추가

## `WEI` 폴더 구조 반영

실제 프로젝트 파일은 현재 저장소 루트 아래 `WEI/`에 있습니다.

훅 스크립트는 이 구조를 감지해서:

- `WEI/`가 있으면 `WEI/logs/user-prompts.jsonl`에 기록
- 없으면 기존처럼 저장소 루트 `logs/user-prompts.jsonl`에 기록

즉, Codex 작업 폴더를 상위 저장소 루트로 열어도 `WEI/` 하위 로그에 정상 기록됩니다.

## 실행 조건

이 프로젝트는 Codex 앱에서 `trusted` 모드로 실행되어야 `.codex` 폴더의 설정 파일을 정상적으로 읽을 수 있습니다.

신뢰되지 않은 상태로 열리면 `.codex`를 읽지 못해서 훅이 로드되지 않거나, 설정한 스크립트가 실행되지 않을 수 있습니다.

## 로그는 어디에 저장되나요?

현재 구조에서는 기본적으로 `WEI/logs/user-prompts.jsonl`에 저장됩니다.

이 경로는 `.gitignore`에 포함되어 있어서 로그 파일 자체는 Git에 올라가지 않습니다.
