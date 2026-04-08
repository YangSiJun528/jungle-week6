# Codex Hooks

이 프로젝트는 Codex의 `UserPromptSubmit` 훅을 사용합니다.

## 무엇을 하나요?

사용자가 프롬프트를 보낼 때마다 `.codex/hooks/user_prompt_submit_log.py`가 실행되어 입력 내용을 JSONL 형식으로 기록합니다.

## 어떻게 설정되어 있나요?

- `.codex/config.toml`: `codex_hooks = true`로 훅 기능 활성화
- `.codex/hooks.json`: `UserPromptSubmit` 이벤트에 Python 스크립트 연결
- `.codex/hooks/user_prompt_submit_log.py`: 표준 입력으로 받은 훅 payload를 로그 파일에 추가

## 실행 조건

이 프로젝트는 Codex 앱에서 `trusted` 모드로 실행되어야 `.codex` 폴더의 설정 파일을 정상적으로 읽을 수 있습니다.

신뢰되지 않은 상태로 열리면 `.codex`를 읽지 못해서 훅이 로드되지 않거나, 설정한 스크립트가 실행되지 않는 문제가 있었습니다.

훅이 동작하지 않으면 먼저 프로젝트가 `trusted` 상태인지 확인하는 것이 좋습니다.

## 로그는 어디에 저장되나요?

로그는 저장소 루트의 `logs/user-prompts.jsonl`에 저장됩니다.

이 경로는 `.gitignore`에 포함되어 있어서 로그 파일 자체는 Git에 올라가지 않습니다.
