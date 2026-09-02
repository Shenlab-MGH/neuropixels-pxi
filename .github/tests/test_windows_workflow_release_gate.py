"""Regression test for the Windows build/publish event matrix."""

from __future__ import annotations

import ast
import re
import sys
from pathlib import Path


WORKFLOW = Path(__file__).parents[1] / "workflows" / "windows.yml"


def job_condition(text: str) -> str:
    match = re.search(r"(?m)^    if: (.+)$", text)
    assert match, "build-windows must have a job-level condition"
    return match.group(1).strip()


def deploy_condition(text: str) -> str:
    deploy = re.search(
        r"(?m)^    - name: deploy\s*\r?\n      if: ([^\r\n]+)$", text
    )
    assert deploy, "deploy must have an explicit step-level condition"
    return deploy.group(1).strip()


def evaluate(expression: str, *, event: str, ref: str, base_ref: str = "", publish: bool = False) -> bool:
    values = {
        "github.event_name": repr(event),
        "github.ref": repr(ref),
        "github.base_ref": repr(base_ref),
        "inputs.publish": repr(publish),
    }
    translated = expression
    for name, value in values.items():
        translated = translated.replace(name, value)
    translated = translated.replace("&&", " and ").replace("||", " or ")
    translated = re.sub(r"\btrue\b", "True", translated)
    translated = re.sub(r"\bfalse\b", "False", translated)
    assert not re.search(r"\b(?:github|inputs)\.", translated), expression
    tree = ast.parse(translated, mode="eval")
    allowed = (ast.Expression, ast.BoolOp, ast.And, ast.Or, ast.Compare, ast.Eq, ast.Constant)
    assert all(isinstance(node, allowed) for node in ast.walk(tree)), expression
    return bool(eval(compile(tree, "<workflow condition>", "eval"), {"__builtins__": {}}, {}))


def main() -> int:
    text = WORKFLOW.read_text(encoding="utf-8")

    triggers_match = re.search(r"(?ms)^on:\s*\n(?P<body>.*?)(?=^[^ \r\n]|\Z)", text)
    assert triggers_match, "workflow triggers are required"
    triggers = triggers_match.group("body")
    assert re.search(r"(?m)^  push:\s*$", triggers), "push builds must remain enabled"
    assert re.search(r"(?m)^  pull_request:\s*$", triggers), "pull-request builds must remain enabled"
    dispatch_match = re.search(
        r"(?ms)^  workflow_dispatch:\s*\n(?P<body>.*?)(?=^  \S|^[^ \r\n]|\Z)", triggers
    )
    assert dispatch_match, "manual dispatch trigger is required"
    dispatch = dispatch_match.group("body")
    assert re.search(r"(?m)^    inputs:\s*$", dispatch), "workflow_dispatch inputs are required"
    assert re.search(r"(?m)^      publish:\s*$", dispatch), "publish input is required"
    assert re.search(r"(?m)^        type: boolean\s*$", dispatch), "publish must be boolean"
    assert re.search(r"(?m)^        default: false\s*$", dispatch), "publish must default to false"

    build = job_condition(text)
    build_matrix = [
        ("push", "refs/heads/main", "", True),
        ("push", "refs/heads/topic", "", True),
        ("pull_request", "refs/pull/2/merge", "main", True),
        ("pull_request", "refs/pull/2/merge", "topic", False),
        ("workflow_dispatch", "refs/heads/main", "", True),
    ]
    for event, ref, base_ref, expected in build_matrix:
        actual = evaluate(build, event=event, ref=ref, base_ref=base_ref)
        assert actual is expected, f"build matrix mismatch: {(event, ref, base_ref)} -> {actual}"

    deploy = deploy_condition(text)
    deploy_matrix = [
        ("push", "refs/heads/main", True, False),
        ("pull_request", "refs/pull/2/merge", True, False),
        ("workflow_dispatch", "refs/heads/main", False, False),
        ("workflow_dispatch", "refs/heads/main", True, True),
        ("workflow_dispatch", "refs/heads/topic", True, False),
    ]
    for event, ref, publish, expected in deploy_matrix:
        actual = evaluate(deploy, event=event, ref=ref, publish=publish)
        assert actual is expected, f"deploy matrix mismatch: {(event, ref, publish)} -> {actual}"

    print("Windows workflow release-gate matrix: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
