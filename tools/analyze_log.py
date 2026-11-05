#!/usr/bin/env python3
"""Checks a binary log written by the logger (sim file or UART capture).

- decodes every 24 byte record and checks sync byte + CRC16
- finds gaps in the sequence numbers (= samples dropped on the device)
- compares that with the dropped / written counters the firmware printed
  in its SUMMARY line (if a console log is given)
- prints period statistics from the timestamps

usage: python3 tools/analyze_log.py log.bin [console.txt]
"""
import re
import struct
import sys

RECORD_SIZE = 24
SYNC = 0xA5


def crc16_ccitt(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def read_records(path):
    with open(path, "rb") as f:
        data = f.read()

    records = []
    crc_errors = 0
    resyncs = 0
    i = 0
    while i + RECORD_SIZE <= len(data):
        if data[i] != SYNC:
            # lost alignment (can happen on a UART capture), look for next sync byte
            i += 1
            resyncs += 1
            continue
        rec = data[i:i + RECORD_SIZE]
        crc_stored = rec[22] | (rec[23] << 8)
        if crc16_ccitt(rec[:22]) != crc_stored:
            crc_errors += 1
            i += 1
            continue
        flags = rec[1]
        seq, t_us = struct.unpack_from("<II", rec, 2)
        ax, ay, az, gx, gy, gz = struct.unpack_from("<6h", rec, 10)
        records.append((seq, t_us, flags, ax, ay, az, gx, gy, gz))
        i += RECORD_SIZE
    return records, crc_errors, resyncs, len(data)


def percentile(sorted_vals, p):
    if not sorted_vals:
        return 0
    k = (len(sorted_vals) - 1) * p / 100.0
    lo = int(k)
    hi = min(lo + 1, len(sorted_vals) - 1)
    return sorted_vals[lo] + (sorted_vals[hi] - sorted_vals[lo]) * (k - lo)


def parse_summary(console_path):
    with open(console_path, errors="replace") as f:
        text = f.read()
    m = None
    for m in re.finditer(r"SUMMARY .*", text):
        pass
    if m is None:
        return None
    fields = dict(re.findall(r"(\w+)=(\d+)", m.group(0)))
    return {k: int(v) for k, v in fields.items()}


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    records, crc_errors, resyncs, nbytes = read_records(sys.argv[1])
    print(f"file bytes        : {nbytes}")
    print(f"records ok        : {len(records)}")
    print(f"crc errors        : {crc_errors}")
    print(f"resync bytes      : {resyncs}")
    if not records:
        sys.exit(1)

    seqs = [r[0] for r in records]
    first, last = seqs[0], seqs[-1]
    out_of_order = sum(1 for a, b in zip(seqs, seqs[1:]) if b <= a)
    gaps = [(a, b - a - 1) for a, b in zip(seqs, seqs[1:]) if b - a > 1]
    missing = sum(n for _, n in gaps)
    print(f"seq range         : {first} .. {last}")
    print(f"out of order      : {out_of_order}")
    print(f"missing (gaps)    : {missing} in {len(gaps)} gap(s)")
    for start, n in gaps[:10]:
        print(f"   gap after seq {start}: {n} sample(s) missing")
    late = sum(1 for r in records if r[2] & 0x02)
    imu_err = sum(1 for r in records if r[2] & 0x01)
    print(f"late flag         : {late}")
    print(f"imu error flag    : {imu_err}")

    # period between consecutive samples that have no gap between them
    periods = [b[1] - a[1] for a, b in zip(records, records[1:]) if b[0] == a[0] + 1]
    periods = [p & 0xFFFFFFFF for p in periods]
    if periods:
        s = sorted(periods)
        mean = sum(s) / len(s)
        print(f"period us mean    : {mean:.1f}")
        print(f"period us p50     : {percentile(s, 50):.0f}")
        print(f"period us p99     : {percentile(s, 99):.0f}")
        print(f"period us p99.9   : {percentile(s, 99.9):.0f}")
        print(f"period us min/max : {s[0]} / {s[-1]}")

    ok = crc_errors == 0 and out_of_order == 0
    if len(sys.argv) > 2:
        summ = parse_summary(sys.argv[2])
        if summ is None:
            print("no SUMMARY line in console log")
            ok = False
        else:
            print(f"device says       : samples={summ.get('samples')} dropped={summ.get('dropped')} "
                  f"written={summ.get('written')}")
            match_written = summ.get("written") == len(records)
            # drops at the very end cannot show as a gap, but the logger drains
            # the queue before SUMMARY so every drop is between two records
            match_dropped = summ.get("dropped") == missing
            print(f"written == records in file : {'YES' if match_written else 'NO'}")
            print(f"dropped == seq gaps        : {'YES' if match_dropped else 'NO'}")
            ok = ok and match_written and match_dropped

    print("RESULT:", "PASS" if ok else "FAIL")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
