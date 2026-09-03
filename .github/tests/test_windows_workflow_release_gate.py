"""Regression test for the Windows build/publish event matrix."""

from __future__ import annotations

import ast
import re
import subprocess
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

    contract_step = text.find("    - name: Test agent inventory contract")
    sop_dependency_step = text.find("    - name: Build pinned OE 1.1 contract dependency")
    sop_integration_step = text.find("    - name: Test NP2 SOP production simulation parity")
    export_step = text.find("    - name: Test Windows plugin exports")
    deploy_step = text.find("    - name: deploy")
    assert export_step >= 0, "Windows CI must verify the built plugin exports"
    assert export_step < contract_step, "plugin exports must pass before standalone contracts"
    assert contract_step >= 0, "Windows CI must run the standalone inventory contract"
    assert contract_step < deploy_step, "inventory contract must pass before deploy"
    assert contract_step < sop_dependency_step < sop_integration_step < deploy_step, (
        "pinned NP2 SOP integration gate must pass after standalone contracts and before deploy"
    )
    contract_commands = text[contract_step:deploy_step]
    assert "cmake -S Tests -B Build-AgentInventory" in contract_commands
    assert "cmake --build Build-AgentInventory --config Release" in contract_commands
    assert "ctest --test-dir Build-AgentInventory -C Release" in contract_commands
    assert "--no-tests=error" in contract_commands

    dependency_commands = text[sop_dependency_step:sop_integration_step]
    pinned_commit = "cd10eb7cae9f9a5a95ca49ce9bf654c9ad29eecc"
    assert "https://github.com/Shenlab-MGH/plugin-GUI.git" in dependency_commands
    assert pinned_commit in dependency_commands
    assert "git checkout --detach" in dependency_commands
    assert "-DBUILD_TESTS=ON" in dependency_commands
    assert "--target gui_testable_source" in dependency_commands

    sop_commands = text[sop_integration_step:deploy_step]
    assert "-DOE_AGENT_BUILD_NP2_SOP_INTEGRATION_TESTS=ON" in sop_commands
    assert "-DGUI_CONTRACT_BUILD_DIR=" in sop_commands
    assert "--target neuropix_agent_np2_sop_map_integration_tests" in sop_commands
    assert "ctest --test-dir Build-Np2SopIntegration -C Release" in sop_commands
    assert "^neuropix_agent_np2_sop_map_integration$" in sop_commands
    assert "--no-tests=error" in sop_commands

    export_commands = text[export_step:contract_step]
    assert "ctest --test-dir Build -C Release" in export_commands
    assert "^neuropix_windows_plugin_exports$" in export_commands
    assert "--no-tests=error" in export_commands

    repo = WORKFLOW.parents[2]
    ignored = subprocess.run(
        ["git", "check-ignore", "--no-index", "Build-AgentInventory/probe.obj"],
        cwd=repo,
        check=False,
        capture_output=True,
        text=True,
    )
    overmatched = subprocess.run(
        ["git", "check-ignore", "--no-index", "Build-AgentInventoryScratch/probe.obj"],
        cwd=repo,
        check=False,
        capture_output=True,
        text=True,
    )
    assert ignored.returncode == 0, "the standard contract build directory must be ignored"
    assert overmatched.returncode == 1, "similarly named source paths must not be ignored"

    source = repo / "Source"
    interface = (source / "UI" / "NeuropixInterface.cpp").read_text(encoding="utf-8")
    update_entry = interface[interface.index("void NeuropixInterface::updateProbeSettingsInBackground()"):
                             interface.index("void NeuropixInterface::comboBoxChanged")]
    assert update_entry.index("tryBeginProbeSettingsWorker") < update_entry.index("updateProbeSettingsQueue")
    assert update_entry.index("waitForThreadToExit (5000)") < update_entry.index("tryBeginProbeSettingsWorker")
    assert "abortProbeSettingsWorker" in update_entry
    assert "probe->updateSettings" not in update_entry, "GUI must not mutate ProbeSettings before owning the gate"

    canvas = (source / "NeuropixCanvas.cpp").read_text(encoding="utf-8")
    updater_constructor = canvas[canvas.index("SettingsUpdater::SettingsUpdater"):
                                 canvas.index("void SettingsUpdater::run")]
    assert updater_constructor.index("tryBeginProbeSettingsWorker") < updater_constructor.index("applyProbeSettings")
    assert "settingsBatch.add" in updater_constructor
    updater_entry = canvas[canvas.index("void SettingsUpdater::run"):]
    assert "applyProbeSettings" not in updater_entry
    assert "ComboBox" not in updater_entry and "repaint" not in updater_entry
    assert updater_entry.index("updateProbeSettingsQueue") < updater_entry.index("startThread")
    assert updater_entry.index("waitForThreadToExit (5000)") > updater_entry.index("startThread")
    assert "abortProbeSettingsWorker" in updater_entry

    editor = (source / "NeuropixEditor.cpp").read_text(encoding="utf-8")
    initialize_entry = editor[editor.index("void NeuropixEditor::initialize"):
                              editor.index("NeuropixEditor::~NeuropixEditor")]
    assert initialize_entry.index("tryBeginProbeSettingsWorker") < initialize_entry.index("uiLoader->startThread")
    assert "abortProbeSettingsWorker" in initialize_entry

    loader_entry = editor[editor.index("void BackgroundLoader::run"):
                          editor.index("void NeuropixEditor::resetCanvas")]
    assert loader_entry.index("SettingsOwnerRelease") < loader_entry.index("initializeBasestations")
    assert "finishProbeSettingsWorker" in loader_entry

    thread = (source / "NeuropixThread.cpp").read_text(encoding="utf-8")
    assert thread.count("updateAgentPresetSettingsQueue (updated)") == 2
    assert "updateProbeSettingsQueue (updated)" not in thread
    assert "|| ! probeSettingsUpdateQueue.isEmpty()" in thread
    assert "probeSettingsUpdateQueue.clear();" in thread

    print("Windows workflow release-gate matrix: PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
