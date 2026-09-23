"""PlatformIO post-build gate for the Stackfall production image.

The gate inspects defined symbols in the linked ELF and the final application
binary size.  It never opens a serial port or writes to a device.
"""

import os
import re
import subprocess

Import("env")  # noqa: F821  (PlatformIO injects this)


APP_CAPACITY_BYTES = 3342336
NM_TOOL = "xtensa-esp32s3-elf-nm"
DEFINED_SYMBOL = re.compile(r"^\s*[0-9A-Fa-f]+\s+([TtWB])\s+(.+?)\s*$")
FORBIDDEN = (
    ("arduinoota", ("ArduinoOTAClass::",)),
    ("mdns", ("MDNSResponder::",)),
    ("httpclient", ("HTTPClient::",)),
    ("wifi_begin", ("WiFiClass::begin",)),
    ("ble", ("BLEDevice::", "NimBLEDevice::")),
)


def fail_tool(message):
    """Exit through SCons without leaking a Python traceback."""
    print("CHECK_IMAGE,FAIL,tool_error,%s" % message)
    env.Exit(1)  # noqa: F821


def resolve_nm():
    found = env.WhereIs(NM_TOOL)  # noqa: F821
    if found:
        return found

    compiler = env.WhereIs(env.subst("$CC"))  # noqa: F821
    if compiler:
        candidate = os.path.join(os.path.dirname(compiler), NM_TOOL)
        if os.path.isfile(candidate):
            return candidate

    fail_tool("nm-not-found")
    return None


def run_nm(nm, elf):
    try:
        result = subprocess.run(
            [nm, "-C", elf], capture_output=True, text=True, check=False
        )
    except OSError as error:
        fail_tool("nm-run-error:%s" % str(error).replace("\n", " "))
        return ""

    if result.returncode != 0:
        detail = (result.stderr or result.stdout).strip().splitlines()
        message = detail[-1] if detail else "no-output"
        fail_tool(
            "nm-exit-%d:%s"
            % (result.returncode, message.replace("\n", " "))
        )
        return ""
    return result.stdout


def defined_symbols(nm_output):
    symbols = []
    for line in nm_output.splitlines():
        match = DEFINED_SYMBOL.match(line)
        if match:
            symbols.append(match.group(2))
    return symbols


def check_image(source, target, env):  # noqa: F811
    del source, target
    elf = env.subst("$BUILD_DIR/${PROGNAME}.elf")  # noqa: F821
    image = env.subst("$BUILD_DIR/${PROGNAME}.bin")  # noqa: F821
    nm_output = run_nm(resolve_nm(), elf)
    symbols = defined_symbols(nm_output)

    try:
        app_bytes = os.path.getsize(image)
    except OSError as error:
        fail_tool("bin-size-error:%s" % str(error).replace("\n", " "))
        return

    matches = []
    for name, needles in FORBIDDEN:
        found = [symbol for symbol in symbols if any(n in symbol for n in needles)]
        matches.append((name, found))
        print("CHECK_IMAGE,%s,%d" % (name, len(found)))
    print("CHECK_IMAGE,symbols,%d" % len(symbols))
    print("CHECK_IMAGE,app_bytes,%d" % app_bytes)

    failed = False
    for name, found in matches:
        for symbol in found:
            print("CHECK_IMAGE,FAIL,%s=%s" % (name, symbol))
            failed = True
    if app_bytes > APP_CAPACITY_BYTES:
        print(
            "CHECK_IMAGE,FAIL,app_bytes=%d>capacity=%d"
            % (app_bytes, APP_CAPACITY_BYTES)
        )
        failed = True

    if failed:
        env.Exit(1)  # noqa: F821
        return
    print("CHECK_IMAGE,PASS,")


# Editing this gate must retrigger the post-build check even without a relink.
env.Depends(  # noqa: F821
    "$BUILD_DIR/${PROGNAME}.bin", env.Value("check_image=1")  # noqa: F821
)
env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_image)  # noqa: F821
