import os
import sys
import time
import traceback

import renderdoc as rd


TARGET_CONNECT_TIMEOUT = 10.0
API_READY_TIMEOUT = 15.0
CAPTURE_TIMEOUT = 30.0


def find_repo_root():
    current = os.path.abspath(os.getcwd())

    while True:
        cmake_file = os.path.join(
            current,
            "CMakeLists.txt"
        )

        tool_manifest = os.path.join(
            current,
            "tools",
            "tool-versions.json"
        )

        if (
            os.path.isfile(cmake_file)
            and os.path.isfile(tool_manifest)
        ):
            return current

        parent = os.path.dirname(current)

        if parent == current:
            break

        current = parent

    raise RuntimeError(
        "Could not find the gsrecon repository root. "
        "Run qrenderdoc from inside the gsrecon repository."
    )


def log(path, message):
    timestamp = time.strftime("%H:%M:%S")

    with open(path, "a", encoding="utf-8") as file:
        file.write(f"[{timestamp}] {message}\n")


def launch(repo_root, capture_dir, log_path):
    executable = os.path.join(
        repo_root,
        "build",
        "windows-msvc",
        "Release",
        "gsrecon.exe",
    )

    if not os.path.isfile(executable):
        raise RuntimeError(
            f"gsrecon executable not found:\n{executable}"
        )

    capture_template = os.path.join(
        capture_dir,
        "gsrecon",
    )

    log(log_path, f"Executable: {executable}")
    log(log_path, "Launching gsrecon with RenderDoc injection...")

    result = pyrenderdoc.Replay().ExecuteAndInject(
        executable,
        os.path.dirname(executable),
        "",
        [],
        capture_template,
        rd.CaptureOptions(),
    )

    if result.result.code != rd.ResultCode.Succeeded:
        raise RuntimeError(
            "ExecuteAndInject failed:\n"
            + result.result.Message()
        )

    if result.ident == 0:
        raise RuntimeError(
            "RenderDoc returned an invalid target identifier."
        )

    log(log_path, f"RenderDoc target ident: {result.ident}")

    return result.ident


def connect(ident, log_path):
    deadline = time.monotonic() + TARGET_CONNECT_TIMEOUT

    while time.monotonic() < deadline:
        target = rd.CreateTargetControl(
            "",
            ident,
            "gsrecon-capture-automation",
            True,
        )

        if target is not None and target.Connected():
            log(log_path, f"Connected to PID {target.GetPID()}")
            return target

        if target is not None:
            target.Shutdown()

        time.sleep(0.1)

    raise RuntimeError(
        "Could not connect to the RenderDoc target."
    )


def wait_for_vulkan(target, log_path):
    log(log_path, "Waiting for Vulkan...")

    deadline = time.monotonic() + API_READY_TIMEOUT

    while time.monotonic() < deadline:
        if not target.Connected():
            raise RuntimeError(
                "gsrecon disconnected before Vulkan became ready."
            )

        # GetAPI() can become valid before we receive RegisterAPI.
        api = str(target.GetAPI())

        if "vulkan" in api.lower():
            log(log_path, f"API registered: {api}")

            # Allow the swapchain/presentation loop to settle.
            time.sleep(0.25)

            log(log_path, "Vulkan capture target is ready.")
            return

        # Pump the target-control connection.
        message = target.ReceiveMessage(None)

        if message is None:
            time.sleep(0.01)
            continue

        if (
            message.type
            == rd.TargetControlMessageType.Disconnected
        ):
            raise RuntimeError(
                "gsrecon disconnected while waiting for Vulkan."
            )

        if (
            message.type
            == rd.TargetControlMessageType.RegisterAPI
        ):
            api = str(target.GetAPI())

            # Only log useful API registration messages.
            if api:
                log(log_path, f"API registered: {api}")

            if "vulkan" in api.lower():
                time.sleep(0.25)

                log(
                    log_path,
                    "Vulkan capture target is ready.",
                )
                return

        # Noop, CapturableWindowCount, etc. are intentionally ignored.

    raise RuntimeError(
        "Timed out waiting for RenderDoc to detect Vulkan."
    )


def capture_frame(target, log_path):
    log(log_path, "Triggering one frame capture...")

    target.TriggerCapture(1)

    deadline = time.monotonic() + CAPTURE_TIMEOUT

    while time.monotonic() < deadline:
        if not target.Connected():
            raise RuntimeError(
                "gsrecon disconnected before capture completed."
            )

        message = target.ReceiveMessage(None)

        if message is None:
            time.sleep(0.01)
            continue

        if (
            message.type
            == rd.TargetControlMessageType.Disconnected
        ):
            raise RuntimeError(
                "gsrecon disconnected while waiting for capture."
            )

        if (
            message.type
            != rd.TargetControlMessageType.NewCapture
        ):
            # Ignore Noop, CaptureProgress,
            # CapturableWindowCount, etc.
            continue

        capture = message.newCapture
        capture_path = str(capture.path)

        if not capture_path:
            raise RuntimeError(
                "RenderDoc reported a capture without a path."
            )

        if not os.path.isfile(capture_path):
            raise RuntimeError(
                "RenderDoc reported a capture, but the file "
                f"does not exist:\n{capture_path}"
            )

        log(log_path, f"Capture frame: {capture.frameNumber}")
        log(log_path, f"Capture path: {capture_path}")
        log(log_path, "RenderDoc capture succeeded.")

        return capture_path

    raise RuntimeError(
        "Timed out waiting for a RenderDoc capture."
    )


def main():
    repo_root = find_repo_root()

    capture_dir = os.path.join(
        repo_root,
        "captures",
        "renderdoc",
    )

    os.makedirs(capture_dir, exist_ok=True)

    log_path = os.path.join(
        capture_dir,
        "automation.log",
    )

    error_path = os.path.join(
        capture_dir,
        "automation-error.txt",
    )

    # Fresh logs for every run.
    with open(log_path, "w", encoding="utf-8") as file:
        file.write("Starting gsrecon RenderDoc automation\n")

    if os.path.isfile(error_path):
        os.remove(error_path)

    log(log_path, f"Repository root: {repo_root}")

    ident = launch(
        repo_root,
        capture_dir,
        log_path,
    )

    target = None

    try:
        target = connect(
            ident,
            log_path,
        )

        wait_for_vulkan(
            target,
            log_path,
        )

        capture_path = capture_frame(
            target,
            log_path,
        )

        log(log_path, "SUCCESS")
        log(log_path, f"Final capture: {capture_path}")

        return 0

    finally:
        if target is not None:
            target.Shutdown()


if __name__ == "__main__":
    try:
        sys.exit(main())

    except Exception:
        try:
            repo_root = find_repo_root()

            error_path = os.path.join(
                repo_root,
                "captures",
                "renderdoc",
                "automation-error.txt",
            )

            with open(
                error_path,
                "w",
                encoding="utf-8",
            ) as file:
                traceback.print_exc(file=file)

        finally:
            sys.exit(1)