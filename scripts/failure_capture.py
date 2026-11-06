#!/usr/bin/env python3
"""Failure capture harness: run one case, emit a verifiable evidence packet.
Stdlib only. Harness evidence proves only that a command ran here with a
recorded result; it is not evidence about any underlying blocker."""
import argparse
import ctypes
import datetime
import hashlib
import json
import os
import platform
import re
import resource
import secrets
import shlex
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

CLAIM_SCOPE = ("harness-evidence-only: proves this command ran here with "
               "this result; not evidence of the Mojo/tcmalloc blocker")
SCHEMA_VERSION = 1
SECRET_KEY_RE = re.compile(r"TOKEN|SECRET|PASSWORD|KEY|CREDENTIAL", re.I)
ERROR_RE = re.compile(r"error:|Error|undefined|fatal")
STAGES = {"fetch", "compile", "link", "tool-startup", "mojo-compile",
          "generated-startup", "runtime", "other"}
PROCESSES = {"build-tool", "mojo-compiler", "mojo-runtime",
             "generated-binary", "aesir", "other"}
TOOLS = ["cc", "gcc", "clang", "ld", "readelf", "bazel", "mojo"]
MAX_TIMEOUT = 300
DEFAULT_TIMEOUT = 60

def utc_now():
    return datetime.datetime.now(datetime.timezone.utc)

def fmt_ts(dt):
    return dt.strftime("%Y-%m-%dT%H:%M:%SZ")

def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()

def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()

def file_info(path):
    p = Path(path)
    return {"sha256": sha256_file(p), "size_bytes": p.stat().st_size}

def run_probe(argv, timeout=5):
    try:
        r = subprocess.run(argv, capture_output=True, text=True,
                           timeout=timeout)
        out = (r.stdout or r.stderr or "").strip().splitlines()
        return out[0].strip() if out else None
    except Exception:
        return None

def read_meminfo():
    keys = ("MemTotal", "MemAvailable", "SwapTotal", "SwapFree")
    vals = {}
    try:
        for line in Path("/proc/meminfo").read_text().splitlines():
            name, _, rest = line.partition(":")
            if name in keys:
                try:
                    vals[name] = int(rest.split()[0])
                except Exception:
                    vals[name] = None
    except Exception:
        pass
    return {k: vals.get(k) for k in keys}

def read_loadavg():
    try:
        parts = Path("/proc/loadavg").read_text().split()
        return [float(parts[0]), float(parts[1]), float(parts[2])]
    except Exception:
        return None

def os_release_pretty():
    try:
        for line in Path("/etc/os-release").read_text().splitlines():
            if line.startswith("PRETTY_NAME="):
                return line.split("=", 1)[1].strip().strip('"')
    except Exception:
        pass
    return None

def libc_info():
    version = None
    try:
        libc = ctypes.CDLL("libc.so.6", use_errno=True)
        fn = libc.gnu_get_libc_version
        fn.restype = ctypes.c_char_p
        raw = fn()
        version = raw.decode() if raw else None
    except Exception:
        version = None
    first = run_probe(["ldd", "--version"])
    return {"ldd_version": first, "gnu_get_libc_version": version,
            "summary": first}

def toolchain_probes():
    out = {}
    for tool in TOOLS:
        path = shutil.which(tool)
        out[tool] = run_probe([path, "--version"]) if path else None
    return out

def is_elf(path):
    try:
        with open(path, "rb") as fh:
            return fh.read(4) == b"\x7fELF"
    except Exception:
        return False

def target_ldd(argv0, cwd):
    try:
        cand = Path(argv0)
        if not cand.is_absolute():
            resolved = shutil.which(argv0)
            if resolved:
                cand = Path(resolved)
            else:
                cand = Path(cwd) / argv0
        if cand.is_file() and is_elf(cand):
            try:
                r = subprocess.run(["ldd", str(cand)], capture_output=True,
                                   text=True, timeout=5)
                return (r.stdout + r.stderr).strip() or None
            except Exception:
                return None
    except Exception:
        pass
    return None

def git_info(repo_root):
    info = {"head_sha": None, "branch": None, "dirty_bool": None}
    def git(*args):
        r = subprocess.run(["git", "-C", str(repo_root)] + list(args),
                           capture_output=True, text=True, timeout=5)
        return r.stdout.strip() if r.returncode == 0 else None
    try:
        info["head_sha"] = git("rev-parse", "HEAD")
        info["branch"] = git("rev-parse", "--abbrev-ref", "HEAD")
        status = git("status", "--porcelain")
        info["dirty_bool"] = bool(status) if status is not None else None
    except Exception:
        pass
    return info

def host_info():
    uname = platform.uname()
    try:
        page_size = os.sysconf("SC_PAGE_SIZE")
    except Exception:
        page_size = None
    try:
        nproc = os.cpu_count()
    except Exception:
        nproc = None
    return {
        "uname": {"sysname": uname.system, "release": uname.release,
                  "machine": uname.machine},
        "os_release_pretty": os_release_pretty(),
        "page_size_bytes": page_size,
        "nproc": nproc,
        "libc": libc_info(),
        "python_version": platform.python_version(),
    }

class HarnessError(Exception):
    pass

def load_and_validate_case(path):
    try:
        raw = Path(path).read_bytes()
    except Exception as exc:
        raise HarnessError(f"cannot read case file: {exc}")
    try:
        case = json.loads(raw.decode("utf-8"))
    except Exception as exc:
        raise HarnessError(f"case is not valid JSON: {exc}")
    if not isinstance(case, dict):
        raise HarnessError("case must be a JSON object")
    if not case.get("id") or not isinstance(case["id"], str):
        raise HarnessError("case.id is required (string)")
    argv = case.get("argv")
    if not isinstance(argv, list) or not argv:
        raise HarnessError("case.argv must be a non-empty ARRAY of strings")
    if not all(isinstance(a, str) for a in argv):
        raise HarnessError("case.argv entries must all be strings")
    env_add = case.get("env_add") or {}
    if not isinstance(env_add, dict):
        raise HarnessError("case.env_add must be an object")
    for key in env_add:
        if SECRET_KEY_RE.search(str(key)):
            raise HarnessError(f"env_add key refused (secret-like): {key}")
    timeout = case.get("timeout_seconds", DEFAULT_TIMEOUT)
    if timeout is None:
        timeout = DEFAULT_TIMEOUT
    if not isinstance(timeout, (int, float)) or timeout <= 0:
        raise HarnessError("timeout_seconds must be a positive number")
    if timeout > MAX_TIMEOUT:
        raise HarnessError("timeout_seconds exceeds hard cap of 300")
    stage = case.get("stage", "other")
    if stage not in STAGES:
        raise HarnessError(f"unknown stage: {stage}")
    proc = case.get("failing_process", "other")
    if proc not in PROCESSES:
        raise HarnessError(f"unknown failing_process: {proc}")
    expected = case.get("expected") or {}
    if not isinstance(expected, dict):
        raise HarnessError("case.expected must be an object")
    outcome = expected.get("outcome")
    if outcome is not None and outcome not in ("pass", "fail"):
        raise HarnessError("expected.outcome must be pass or fail")
    cwd = case.get("cwd") or "."
    if not isinstance(cwd, str) or Path(cwd).is_absolute():
        raise HarnessError("case.cwd must be repo-root-relative")
    return case, raw

def scan_errors(stdout_path, stderr_path):
    first_relevant = None
    error_lines = []
    for stream, path in (("stderr", stderr_path), ("stdout", stdout_path)):
        try:
            text = Path(path).read_text(errors="replace")
        except Exception:
            continue
        for idx, line in enumerate(text.splitlines(), start=1):
            if stream == "stderr" and first_relevant is None and line.strip():
                first_relevant = line.strip()[:500]
            if len(error_lines) < 5 and ERROR_RE.search(line):
                error_lines.append({"stream": stream, "line_no": idx,
                                    "text": line.strip()[:500]})
    return first_relevant, error_lines

def repo_root():
    here = Path(__file__).resolve()
    if here.parent.name == "scripts":
        return here.parent.parent
    return Path.cwd()

def cmd_run(args):
    case, raw = load_and_validate_case(args.case)
    root = repo_root()
    timeout = case.get("timeout_seconds") or DEFAULT_TIMEOUT
    if args.timeout is not None:
        if args.timeout <= 0 or args.timeout > MAX_TIMEOUT:
            raise HarnessError("--timeout must be in 1..300")
        timeout = args.timeout
    child_cwd = (root / (case.get("cwd") or ".")).resolve()
    if not child_cwd.is_dir():
        raise HarnessError(f"case cwd does not exist: {case.get('cwd')}")
    case_id = re.sub(r"[^A-Za-z0-9._-]", "-", case["id"])
    run_id = "%s-%s-%s" % (utc_now().strftime("%Y%m%dT%H%M%SZ"), case_id,
                           secrets.token_hex(3))
    out_root = Path(args.out_root)
    if not out_root.is_absolute():
        out_root = root / out_root
    run_dir = out_root / run_id
    run_dir.mkdir(parents=True, exist_ok=False)
    (run_dir / "case.json").write_bytes(raw)
    (run_dir / "command.txt").write_text(shlex.join(case["argv"]) + "\n")
    env = dict(os.environ)
    for k, v in (case.get("env_add") or {}).items():
        env[str(k)] = str(v)
    host_before = read_meminfo()
    load_before = read_loadavg()
    try:
        df_free = shutil.disk_usage(child_cwd).free
    except Exception:
        df_free = None
    started = utc_now()
    t0 = time.monotonic()
    exit_code = None
    sig = None
    timed_out = False
    stdout_path = run_dir / "stdout.log"
    stderr_path = run_dir / "stderr.log"
    with open(stdout_path, "wb") as out_f, open(stderr_path, "wb") as err_f:
        try:
            proc = subprocess.Popen(case["argv"], cwd=str(child_cwd), env=env,
                                    stdout=out_f, stderr=err_f,
                                    start_new_session=True)
        except Exception as exc:
            raise HarnessError(f"spawn failure: {exc}")
        try:
            rc = proc.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except Exception:
                try:
                    proc.kill()
                except Exception:
                    pass
            rc = proc.wait()
    duration_ms = int(round((time.monotonic() - t0) * 1000))
    finished = utc_now()
    if timed_out:
        sig = int(signal.SIGKILL)
    elif rc is not None and rc < 0:
        sig = -rc
    else:
        exit_code = rc
    try:
        peak_kb = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
    except Exception:
        peak_kb = None
    outcome = "pass" if (not timed_out and exit_code == 0) else "fail"
    expected = case.get("expected") or {}
    exp_outcome = expected.get("outcome")
    exp_code = expected.get("exit_code")
    matches = True
    if exp_outcome is not None and exp_outcome != outcome:
        matches = False
    if exp_code is not None and exp_code != exit_code:
        matches = False
    first_err, err_lines = scan_errors(stdout_path, stderr_path)
    host = host_info()
    manifest = {
        "schema_version": case.get("schema_version", SCHEMA_VERSION),
        "run_id": run_id, "claim_scope": CLAIM_SCOPE,
        "case_id": case["id"], "title": case.get("title"),
        "stage": case.get("stage", "other"),
        "failing_process": case.get("failing_process", "other"),
        "classification_hint": case.get("classification_hint"),
        "notes": case.get("notes"), "classification": "unclassified",
        "argv": case["argv"], "cwd": case.get("cwd") or ".",
        "started_at_utc": fmt_ts(started),
        "finished_at_utc": fmt_ts(finished), "duration_ms": duration_ms,
        "exit_code": exit_code, "signal": sig, "timed_out": timed_out,
        "outcome": outcome, "matches_expected": matches,
        "expected": expected, "first_relevant_error": first_err,
        "error_lines": err_lines, "env_class": case.get("env_class") or "arm64-linux",
        "uname": host["uname"], "os_release_pretty": host["os_release_pretty"],
        "page_size_bytes": host["page_size_bytes"], "nproc": host["nproc"],
        "libc": host["libc"], "python_version": host["python_version"],
        "toolchain": toolchain_probes(),
        "target_ldd": target_ldd(case["argv"][0], child_cwd),
        "meminfo_before": host_before, "meminfo_after": read_meminfo(),
        "loadavg_before": load_before, "df_free_bytes_cwd": df_free,
        "child_peak_rss_kb": peak_kb, "git": git_info(root),
        "harness_sha256": sha256_file(Path(__file__).resolve()),
        "case_sha256": sha256_bytes(raw),
        "logs": {"stdout.log": file_info(stdout_path),
                 "stderr.log": file_info(stderr_path)},
        "stdout_log": file_info(stdout_path),
        "stderr_log": file_info(stderr_path),
    }
    (run_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n")
    names = ["manifest.json", "case.json", "command.txt", "stdout.log",
             "stderr.log"]
    lines = ["%s  %s" % (sha256_file(run_dir / n), n) for n in names]
    (run_dir / "SHA256SUMS").write_text("\n".join(lines) + "\n")
    print(str(run_dir))
    print(json.dumps({"run_id": run_id, "outcome": outcome,
                      "matches_expected": matches, "timed_out": timed_out,
                      "exit_code": exit_code, "signal": sig}, sort_keys=True))
    if timed_out:
        return 4
    return 0 if matches else 2

def cmd_verify(args):
    run_dir = Path(args.run_dir)
    problems = []
    def check(ok, label):
        print(("OK   " if ok else "FAIL ") + label)
        if not ok:
            problems.append(label)
    if not run_dir.is_dir():
        print(f"FAIL run dir missing: {run_dir}")
        return 3
    try:
        manifest = json.loads((run_dir / "manifest.json").read_text())
        case = json.loads((run_dir / "case.json").read_text())
    except Exception as exc:
        print(f"FAIL cannot load manifest/case: {exc}")
        return 3
    sums = {}
    try:
        for line in (run_dir / "SHA256SUMS").read_text().splitlines():
            if line.strip():
                digest, name = line.split(None, 1)
                sums[name.strip().lstrip("*")] = digest
    except Exception as exc:
        check(False, f"SHA256SUMS unreadable: {exc}")
    for name in ("manifest.json", "case.json", "command.txt", "stdout.log", "stderr.log"):
        p = run_dir / name
        check(p.is_file() and sums.get(name) == sha256_file(p), f"sha256 {name}")
    logs = manifest.get("logs") or {}
    for name in ("stdout.log", "stderr.log"):
        info = logs.get(name) or {}
        p = run_dir / name
        ok = p.is_file() and info.get("sha256") == sha256_file(p) and info.get("size_bytes") == p.stat().st_size
        check(bool(ok), f"manifest log entry {name}")
    check(manifest.get("case_sha256") == sha256_file(run_dir / "case.json"), "case_sha256")
    exp = case.get("expected") or {}
    exp_matches = True
    if exp.get("outcome") is not None and exp.get("outcome") != manifest.get("outcome"):
        exp_matches = False
    if exp.get("exit_code") is not None and exp.get("exit_code") != manifest.get("exit_code"):
        exp_matches = False
    check(manifest.get("matches_expected") == exp_matches, "matches_expected consistency")
    check(str(manifest.get("started_at_utc", "")) <= str(manifest.get("finished_at_utc", "")), "timestamp order")
    host = host_info()
    check(manifest.get("uname", {}).get("machine") == host["uname"]["machine"], "host machine arch")
    check(manifest.get("page_size_bytes") == host["page_size_bytes"], "host page size")
    check(manifest.get("os_release_pretty") == host["os_release_pretty"], "host OS pretty")
    old_libc = (manifest.get("libc") or {}).get("gnu_get_libc_version")
    check(old_libc == host["libc"]["gnu_get_libc_version"], "host libc version")
    print("VERIFY " + ("PASS" if not problems else f"FAIL ({len(problems)} problem(s))"))
    return 0 if not problems else 2

def main(argv=None):
    parser = argparse.ArgumentParser(prog="failure_capture.py")
    sub = parser.add_subparsers(dest="cmd", required=True)
    p_run = sub.add_parser("run")
    p_run.add_argument("--case", required=True)
    p_run.add_argument("--out-root", default="evidence/runs")
    p_run.add_argument("--timeout", type=int, default=None)
    p_ver = sub.add_parser("verify")
    p_ver.add_argument("run_dir")
    args = parser.parse_args(argv)
    try:
        return cmd_run(args) if args.cmd == "run" else cmd_verify(args)
    except HarnessError as exc:
        print(f"harness error: {exc}", file=sys.stderr)
        return 3

if __name__ == "__main__":
    sys.exit(main())
