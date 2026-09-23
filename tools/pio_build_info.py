Import("env")
import os
import subprocess


def _read_version():
    # Repo-root VERSION file; task 7 creates it, so fall back until then.
    # NOTE: extra_scripts run exec'd by SCons, so __file__ is undefined here;
    # PROJECT_DIR is the supported way to locate the repo root.
    path = os.path.join(env.get("PROJECT_DIR", "."), "VERSION")
    try:
        with open(path, "r") as f:
            ver = f.read().strip()
        return ver if ver else "0.0.0"
    except Exception:
        return "0.0.0"


def _read_sha():
    try:
        out = subprocess.check_output(["git", "rev-parse", "--short", "HEAD"])
        return out.decode("utf-8").strip() or "nogit"
    except Exception:
        return "nogit"


env.Append(
    CPPDEFINES=[
        ("SF_VERSION", env.StringifyMacro(_read_version())),
        ("SF_SHA", env.StringifyMacro(_read_sha())),
        ("SF_BUILD_ID", env.StringifyMacro(_read_sha())),
    ]
)
