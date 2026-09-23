#!/usr/bin/env python3
"""Split Stackfall IMU logs into fixtures, or create hypothesis fixtures.

Raw device rows are copied verbatim from this form:
  IMU,<t_ms>,<t_us>,<STAGE>,<ax_g>,<ay_g>,<az_g>

Usage:
  make_trace_fixture.py RAW_LOG [RAW_LOG ...] [--out traces/]
  make_trace_fixture.py --synthesize traces/synthetic/
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple


SCHEMA = "stackfall-trace-labels/1"
BOARD = "M5Stack StickS3 (BMI270)"
CSV_HEADER = "t_ms,t_us,ax_g,ay_g,az_g"
LOG_HEADER = "IMU_HEADER,t_ms,t_us,stage,ax_g,ay_g,az_g"
STAGES = (
    "REST",
    "INTENT_LEFT",
    "INTENT_RIGHT",
    "PRECISION",
    "CROSS_AXIS",
    "SUSTAINED",
)
MIN_ROWS = 3000
SYNTH_ROWS = 5000
SYNTH_DT_MS = 6


class FixtureError(Exception):
    pass


@dataclass
class RawBlock:
    session: int
    stage: str
    rows: List[str] = field(default_factory=list)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def expectations(stage: str) -> Dict[str, Dict[str, int]]:
    common: Dict[str, Dict[str, int]] = {
        "MoveWhileDip": {"eq": 0},
        "dropped": {"eq": 0},
    }
    by_stage: Dict[str, Dict[str, Dict[str, int]]] = {
        "REST": {
            "MoveLeft": {"eq": 0},
            "MoveRight": {"eq": 0},
            "SoftDropOn": {"eq": 0},
            "RotateCw": {"eq": 0},
        },
        "INTENT_LEFT": {
            "MoveLeft": {"ge": 3},
            "MoveRight": {"eq": 0},
            "SoftDropOn": {"eq": 0},
        },
        "INTENT_RIGHT": {
            "MoveLeft": {"eq": 0},
            "MoveRight": {"ge": 3},
            "SoftDropOn": {"eq": 0},
        },
        "PRECISION": {
            "MoveLeft": {"eq": 1},
            "MoveRight": {"eq": 0},
            "SoftDropOn": {"eq": 0},
        },
        "CROSS_AXIS": {
            "MoveRight": {"ge": 3},
            "SoftDropOn": {"eq": 0},
            "RotateCw": {"eq": 0},
        },
        "SUSTAINED": {
            "MoveLeft": {"ge": 2},
            "MoveRight": {"ge": 2},
            "RotateCw": {"eq": 1},
            "SoftDropOn": {"eq": 1},
            "SoftDropOff": {"eq": 1},
        },
    }
    result = dict(by_stage[stage])
    result.update(common)
    return result


def label_text(
    name: str,
    stage: str,
    capture_date: Optional[str],
    synthetic: bool,
    source_hash: str,
) -> str:
    labels = {
        "schema": SCHEMA,
        "name": name,
        "stage": stage,
        "capture_date": capture_date,
        "board": BOARD,
        "evidence": "HYPOTHESIS" if synthetic else "DEVICE_CAPTURE",
        "provenance": {
            "synthetic": synthetic,
            "source_sha256": source_hash,
        },
        "expect": expectations(stage),
    }
    return json.dumps(labels, indent=2, sort_keys=True) + "\n"


def write_same_or_new(path: Path, text: str) -> None:
    if path.exists() and path.read_text(encoding="utf-8") != text:
        raise FixtureError(f"{path}: exists with different content")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_fixture(
    out_dir: Path,
    name: str,
    stage: str,
    capture_date: Optional[str],
    synthetic: bool,
    source_hash: str,
    csv_text: str,
) -> None:
    write_same_or_new(out_dir / f"{name}.csv", csv_text)
    write_same_or_new(
        out_dir / f"{name}.labels.json",
        label_text(name, stage, capture_date, synthetic, source_hash),
    )


def parse_u32(token: str, context: str) -> int:
    if not re.fullmatch(r"[0-9]+", token):
        raise FixtureError(f"{context}: invalid unsigned integer {token!r}")
    value = int(token)
    if value > 0xFFFFFFFF:
        raise FixtureError(f"{context}: value exceeds uint32")
    return value


def validate_float(token: str, context: str) -> None:
    if token.strip() != token:
        raise FixtureError(f"{context}: whitespace would violate verbatim CSV")
    try:
        value = float(token)
    except ValueError as exc:
        raise FixtureError(f"{context}: invalid float {token!r}") from exc
    if not math.isfinite(value):
        raise FixtureError(f"{context}: non-finite float {token!r}")


def derive_date(path: Path, explicit: Optional[str]) -> str:
    if explicit is not None:
        if not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", explicit):
            raise FixtureError("--capture-date must be YYYY-MM-DD")
        return explicit
    match = re.search(r"(20[0-9]{2})([01][0-9])([0-3][0-9])", path.name)
    if match is None:
        raise FixtureError(f"{path}: cannot derive date; pass --capture-date YYYY-MM-DD")
    return f"{match.group(1)}-{match.group(2)}-{match.group(3)}"


def split_log(path: Path) -> Tuple[List[RawBlock], str]:
    raw = path.read_bytes()
    try:
        lines = raw.decode("utf-8").splitlines()
    except UnicodeDecodeError as exc:
        raise FixtureError(f"{path}: log is not UTF-8") from exc

    if sum(line == LOG_HEADER for line in lines) != 1:
        raise FixtureError(f"{path}: expected exactly one {LOG_HEADER}")

    heartbeat = []
    for line in lines:
        match = re.search(r"\[HB\]\s+n=([0-9]+)", line)
        if match:
            heartbeat.append(int(match.group(1)))
    if len(heartbeat) < 2 or not any(b > a for a, b in zip(heartbeat, heartbeat[1:])):
        raise FixtureError(
            f"{path}: live transcript not proven by two increasing [HB] n= counters"
        )

    blocks: List[RawBlock] = []
    current: Optional[RawBlock] = None
    session = 0
    seen: set[str] = set()
    last_ms: Optional[int] = None
    for line_no, line in enumerate(lines, 1):
        if line.startswith("EVENT,TILT_PROBE_STARTED,"):
            fields = line.split(",")
            if len(fields) != 3 or fields[2] not in STAGES:
                raise FixtureError(f"{path}:{line_no}: invalid stage boundary")
            stage = fields[2]
            if session == 0 or stage == "REST" or stage in seen:
                session += 1
                seen = set()
            seen.add(stage)
            current = RawBlock(session, stage)
            blocks.append(current)
            continue
        if not line.startswith("IMU,"):
            continue
        if current is None:
            raise FixtureError(f"{path}:{line_no}: IMU row precedes a stage boundary")
        fields = line.split(",")
        if len(fields) != 7 or fields[3] != current.stage:
            raise FixtureError(f"{path}:{line_no}: malformed or mismatched IMU row")
        now_ms = parse_u32(fields[1], f"{path}:{line_no}")
        parse_u32(fields[2], f"{path}:{line_no}")
        for token in fields[4:7]:
            validate_float(token, f"{path}:{line_no}")
        if last_ms is not None and now_ms < last_ms:
            raise FixtureError(f"{path}:{line_no}: t_ms decreased")
        last_ms = now_ms
        # These five numeric tokens are copied from the device line byte for byte.
        current.rows.append(",".join((fields[1], fields[2], fields[4], fields[5], fields[6])))

    if not blocks:
        raise FixtureError(f"{path}: no trace stages found")
    sessions = sorted({block.session for block in blocks})
    for number in sessions:
        stage_blocks = [block for block in blocks if block.session == number]
        if tuple(block.stage for block in stage_blocks) != STAGES:
            raise FixtureError(
                f"{path}: session {number} does not contain the six stages in order"
            )
        for block in stage_blocks:
            if len(block.rows) < MIN_ROWS:
                raise FixtureError(
                    f"{path}: session {number} {block.stage} has {len(block.rows)} rows; "
                    f"need >= {MIN_ROWS}"
                )
    return blocks, sha256(raw)


def convert_log(path: Path, out_dir: Path, capture_date: Optional[str]) -> int:
    date = derive_date(path, capture_date)
    blocks, source_hash = split_log(path)
    for block in blocks:
        name = f"{date.replace('-', '')}-{block.session}-{block.stage}"
        csv_text = CSV_HEADER + "\n" + "\n".join(block.rows) + "\n"
        write_fixture(out_dir, name, block.stage, date, False, source_hash, csv_text)
        print(f"FIXTURE {name} stage={block.stage} rows={len(block.rows)} source=CAPTURE")
    return len(blocks)


def ramp(index: int, start: int, end: int, target: float) -> float:
    if index <= start:
        return 0.0
    if index >= end:
        return target
    return target * float(index - start) / float(end - start)


def block_noise(index: int, amplitude: float, offset: int = 0) -> float:
    if index == 0:
        return 0.0
    levels = (amplitude, -amplitude, amplitude * 0.5, -amplitude * 0.5)
    return levels[((index + offset) // 250) % len(levels)]


def sustained_signals(index: int) -> Tuple[float, float]:
    signed_y = 0.0
    if 500 < index < 1300:
        signed_y = ramp(index, 500, 800, 0.27)
    elif 1300 <= index < 1700:
        signed_y = 0.27 - ramp(index, 1300, 1650, 0.27)

    z = 0.0
    if 2000 < index < 2800:
        z = ramp(index, 2000, 2150, 0.70)
        if index >= 2650:
            z = 0.70 - ramp(index, 2650, 2800, 0.70)
        if 2250 <= index < 2500:
            signed_y = -0.35
    elif 3200 < index < 4000:
        z = -ramp(index, 3200, 3350, 0.70)
        if index >= 3850:
            z = -0.70 + ramp(index, 3850, 4000, 0.70)
        if 3450 <= index < 3700:
            signed_y = 0.35
    return signed_y, z


def synth_sample(stage: str, index: int) -> Tuple[float, float, float]:
    if stage == "REST":
        return (
            -1.0 + block_noise(index, 0.005, 50),
            block_noise(index, 0.019),
            block_noise(index, 0.019, 125),
        )

    signed_y = 0.0
    z = 0.0
    ax = -1.0
    if stage == "INTENT_LEFT":
        signed_y = ramp(index, 100, 600, -0.35)
        z = 0.067 * min(1.0, abs(signed_y) / 0.35)
    elif stage == "INTENT_RIGHT":
        signed_y = ramp(index, 100, 600, 0.35)
        z = 0.067 * min(1.0, abs(signed_y) / 0.35)
    elif stage == "PRECISION":
        signed_y = ramp(index, 100, 600, -0.13)
    elif stage == "CROSS_AXIS":
        signed_y = ramp(index, 100, 600, 0.30)
        z = 0.067 * min(1.0, abs(signed_y) / 0.30)
        ax += block_noise(index, 0.10)
    elif stage == "SUSTAINED":
        signed_y, z = sustained_signals(index)

    # kTiltSignLeftRight is -1: positive signed steering is negative raw Y.
    return ax, -signed_y, z


def synthesize(out_dir: Path) -> int:
    count = 0
    for stage in STAGES:
        rows = [CSV_HEADER]
        for index in range(SYNTH_ROWS):
            ax, ay, az = synth_sample(stage, index)
            rows.append(
                f"{index * SYNTH_DT_MS},{index * SYNTH_DT_MS * 1000},"
                f"{ax:.6f},{ay:.6f},{az:.6f}"
            )
        csv_text = "\n".join(rows) + "\n"
        name = f"hypothesis-{stage.lower()}-synth"
        source_hash = sha256(csv_text.encode("ascii"))
        write_fixture(out_dir, name, stage, None, True, source_hash, csv_text)
        print(f"FIXTURE {name} stage={stage} rows={SYNTH_ROWS} source=HYPOTHESIS")
        count += 1
    print(f"SUMMARY fixtures={count} synthetic={count}")
    return count


def main(argv: Optional[Sequence[str]] = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", help="raw device-monitor logs")
    parser.add_argument("--out", default="traces", help="directory for captured fixtures")
    parser.add_argument("--capture-date", help="YYYY-MM-DD when the filename has no date")
    parser.add_argument(
        "--synthesize", metavar="DIR", help="write six deterministic hypothesis fixtures"
    )
    args = parser.parse_args(argv)
    try:
        if args.synthesize is not None:
            if args.inputs or args.capture_date is not None or args.out != "traces":
                raise FixtureError(
                    "--synthesize cannot be combined with inputs, --out, or --capture-date"
                )
            synthesize(Path(args.synthesize))
            return 0
        if not args.inputs:
            parser.print_usage(sys.stderr)
            return 2
        total = 0
        for raw_path in args.inputs:
            total += convert_log(Path(raw_path), Path(args.out), args.capture_date)
        print(f"SUMMARY fixtures={total} synthetic=0")
        return 0
    except (FixtureError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
