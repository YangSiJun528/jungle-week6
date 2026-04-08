#!/usr/bin/env python3
"""Append every UserPromptSubmit payload to a project-local JSONL log."""

from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
LOG_PATH = REPO_ROOT / "logs" / "user-prompts.jsonl"


def main() -> int:
    try:
        payload = json.load(sys.stdin)
    except json.JSONDecodeError as exc:
        print(f"UserPromptSubmit hook received invalid JSON: {exc}", file=sys.stderr)
        return 0

    log_entry = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "session_id": payload.get("session_id"),
        "turn_id": payload.get("turn_id"),
        "cwd": payload.get("cwd"),
        "model": payload.get("model"),
        "hook_event_name": payload.get("hook_event_name"),
        "prompt": payload.get("prompt", ""),
    }

    try:
        LOG_PATH.parent.mkdir(parents=True, exist_ok=True)
        with LOG_PATH.open("a", encoding="utf-8") as log_file:
            log_file.write(json.dumps(log_entry, ensure_ascii=False))
            log_file.write("\n")
    except OSError as exc:
        print(f"Failed to append prompt log: {exc}", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
