#!/usr/bin/env python3
"""Impersonates the Livox-SDK2 host to validate the Mid-360 emulator without ROS.

Phases (each prints PASS/FAIL; exit code 0 only if all pass):
  1. discover  - broadcast 0x0000 from host_ip:56000, validate detection ACK
  2. inquire   - 0x0101 fw_type (0x8010) from host_ip:56101, require non-zero
  3. configure - 0x0100 host endpoints + the 5 single-key writes the ROS driver
                 sends; require ret=0/error_key=0 acks with echoed seq
  4. measure   - verify point (~2083.3 pkt/s) and IMU (200 Hz) streams: source
                 ports, exact length fields, dot_num, gapless udp_cnt

Uses only the stdlib; CRCs via binascii.crc_hqx (CRC-16/CCITT-FALSE) and
zlib.crc32 — independent of the C++ implementation.
"""
import argparse
import binascii
import math
import select
import socket
import struct
import sys
import time
import zlib

SOF = 0xAA


def pack_ctrl(seq, cmd_id, cmd_type, sender_type, payload=b""):
    hdr = struct.pack("<BBHIHBB6s", SOF, 0, 24 + len(payload), seq, cmd_id,
                      cmd_type, sender_type, b"\x00" * 6)
    crc16 = binascii.crc_hqx(hdr, 0xFFFF)
    crc32 = zlib.crc32(payload) if payload else 0
    return hdr + struct.pack("<HI", crc16, crc32) + payload


def parse_ctrl(buf):
    if len(buf) < 24:
        return None, "short frame"
    sof, ver, length, seq, cmd_id, cmd_type, sender = struct.unpack("<BBHIHBB", buf[:12])
    crc16, crc32 = struct.unpack("<HI", buf[18:24])
    if sof != SOF or ver != 0:
        return None, f"bad sof/version {sof:#x}/{ver}"
    if length < 24 or length > len(buf):
        return None, f"bad length field {length} (datagram {len(buf)})"
    if binascii.crc_hqx(buf[:18], 0xFFFF) != crc16:
        return None, "header crc16 mismatch"
    payload = buf[24:length]
    expect = zlib.crc32(payload) if payload else 0
    if expect != crc32:
        return None, "payload crc32 mismatch"
    return dict(seq=seq, cmd_id=cmd_id, cmd_type=cmd_type, sender=sender,
                payload=payload), None


class Phase:
    def __init__(self):
        self.failures = []

    def check(self, cond, what):
        if not cond:
            self.failures.append(what)
        return cond


def wait_reply(sock, seq, timeout):
    """Receive until a CRC-valid ACK with matching seq arrives or timeout."""
    deadline = time.monotonic() + timeout
    while True:
        remain = deadline - time.monotonic()
        if remain <= 0:
            return None, None, "timeout"
        r, _, _ = select.select([sock], [], [], remain)
        if not r:
            return None, None, "timeout"
        buf, addr = sock.recvfrom(4096)
        frame, err = parse_ctrl(buf)
        if frame is None:
            return None, None, f"unparseable reply: {err}"
        if frame["cmd_type"] == 1 and frame["seq"] == seq:
            return frame, addr, None
        # ignore unrelated traffic (e.g. state pushes) and keep waiting


def phase_discover(args, seq):
    p = Phase()
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.bind((args.host_ip, 56000))
    frame = None
    for attempt in range(args.attempts):
        dst = args.lidar_ip if args.unicast_discovery else "255.255.255.255"
        s.sendto(pack_ctrl(seq, 0x0000, 0, 0), (dst, 56000))
        frame, addr, err = wait_reply(s, seq, 1.0)
        if frame:
            break
        print(f"  discovery attempt {attempt + 1}: {err}")
    if not p.check(frame is not None, "no detection ACK"):
        s.close()
        return p, None
    p.check(addr[0] == args.lidar_ip, f"ACK source IP {addr[0]} != {args.lidar_ip}")
    p.check(addr[1] == 56000, f"ACK source port {addr[1]} != 56000")
    p.check(frame["sender"] == 1, "sender_type != lidar")
    pl = frame["payload"]
    if p.check(len(pl) == 24, f"detection payload {len(pl)} != 24 bytes"):
        ret, dev_type = pl[0], pl[1]
        sn = pl[2:18].rstrip(b"\x00").decode(errors="replace")
        ip = ".".join(str(b) for b in pl[18:22])
        cmd_port = struct.unpack("<H", pl[22:24])[0]
        p.check(ret == 0, f"ret_code {ret}")
        p.check(dev_type == 9, f"dev_type {dev_type} != 9 (Mid-360)")
        p.check(ip == args.lidar_ip, f"payload lidar_ip {ip} != {args.lidar_ip}")
        p.check(cmd_port == 56100, f"cmd_port {cmd_port} != 56100")
        print(f"  detected sn={sn} ip={ip} cmd_port={cmd_port}")
    s.close()
    return p, frame


def send_cmd(sock, args, seq, cmd_id, payload, p, what):
    sock.sendto(pack_ctrl(seq, cmd_id, 0, 0, payload), (args.lidar_ip, 56100))
    frame, addr, err = wait_reply(sock, seq, 2.0)
    if not p.check(frame is not None, f"{what}: {err}"):
        return None
    p.check(addr == (args.lidar_ip, 56100),
            f"{what}: ack from {addr} not {(args.lidar_ip, 56100)}")
    return frame


def phase_inquire(args, cmd_sock, seq):
    p = Phase()
    # The SDK's exact (bare-key) inquire for fw_type.
    payload = struct.pack("<HHH", 1, 0, 0x8010)
    frame = send_cmd(cmd_sock, args, seq, 0x0101, payload, p, "fw_type inquire")
    if frame:
        pl = frame["payload"]
        ok = p.check(len(pl) >= 8, f"read ack too short ({len(pl)})")
        if ok:
            ret, num = pl[0], struct.unpack("<H", pl[1:3])[0]
            key, klen = struct.unpack("<HH", pl[3:7])
            fw = pl[7]
            p.check(ret == 0, f"ret_code {ret}")
            p.check(num == 1 and key == 0x8010 and klen == 1, "unexpected KV shape")
            p.check(fw != 0, "fw_type is 0 (bootloader) — handshake would die here")
            print(f"  fw_type = {fw}")
    return p


def kv_entry(key, value):
    return struct.pack("<HH", key, len(value)) + value


def check_write_ack(frame, p, what):
    if frame is None:
        return
    pl = frame["payload"]
    if p.check(len(pl) >= 3, f"{what}: write ack too short"):
        ret = pl[0]
        error_key = struct.unpack("<H", pl[1:3])[0]
        p.check(ret == 0, f"{what}: ret_code {ret}")
        p.check(error_key == 0, f"{what}: error_key {error_key:#x}")


def phase_configure(args, cmd_sock, seqs):
    p = Phase()
    host = bytes(int(x) for x in args.host_ip.split("."))

    def ep(host_port, lidar_port):
        return host + struct.pack("<HH", host_port, lidar_port)

    body = struct.pack("<HH", 3, 0)
    body += kv_entry(0x0005, ep(56201, 56200))
    body += kv_entry(0x0006, ep(56301, 56300))
    body += kv_entry(0x0007, ep(56401, 56400))
    check_write_ack(send_cmd(cmd_sock, args, next(seqs), 0x0100, body, p,
                             "host endpoints"), p, "host endpoints")

    attitude = struct.pack("<fffiii", 0.0, 0.0, 0.0, 0, 0, 0)
    for what, key, value in [("pcl_data_type", 0x0000, b"\x01"),
                             ("pattern_mode", 0x0001, b"\x00"),
                             ("install_attitude", 0x0012, attitude),
                             ("work_mode", 0x001A, b"\x01"),
                             ("imu_data_en", 0x001C, b"\x01")]:
        body = struct.pack("<HH", 1, 0) + kv_entry(key, value)
        check_write_ack(send_cmd(cmd_sock, args, next(seqs), 0x0100, body, p, what),
                        p, what)
    return p


class StreamStats:
    def __init__(self, name, want_dtype, want_len, want_port, want_dot):
        self.name = name
        self.want_dtype = want_dtype
        self.want_len = want_len
        self.want_port = want_port
        self.want_dot = want_dot
        self.count = 0
        self.bad = []
        self.last_udp_cnt = None
        self.gaps = 0
        self.first_t = None
        self.last_t = None
        self.acc_norm_sum = 0.0

    def feed(self, buf, addr, lidar_ip):
        now = time.monotonic()
        if self.first_t is None:
            self.first_t = now
        self.last_t = now
        self.count += 1
        if len(self.bad) < 5:  # cap noise
            if addr != (lidar_ip, self.want_port):
                self.bad.append(f"source {addr} != {(lidar_ip, self.want_port)}")
                return
            if len(buf) < 36:
                self.bad.append(f"short datagram {len(buf)}")
                return
        (ver, length, _ti, dot, udp_cnt, _fc, dtype, ttype) = struct.unpack(
            "<BHHHHBBB", buf[:11])
        if len(self.bad) < 5:
            if ver != 0:
                self.bad.append(f"version {ver}")
            if length != len(buf):
                self.bad.append(f"length field {length} != datagram {len(buf)}")
            if length != self.want_len:
                self.bad.append(f"length {length} != {self.want_len}")
            if dtype != self.want_dtype:
                self.bad.append(f"data_type {dtype}")
            if ttype != 0:
                self.bad.append(f"time_type {ttype} != 0")
            if dot != self.want_dot:
                self.bad.append(f"dot_num {dot} != {self.want_dot}")
        if self.last_udp_cnt is not None and udp_cnt != (self.last_udp_cnt + 1) % 65536:
            self.gaps += 1
        self.last_udp_cnt = udp_cnt
        if self.want_dtype == 0 and len(buf) >= 60:
            gx, gy, gz, ax, ay, az = struct.unpack("<6f", buf[36:60])
            self.acc_norm_sum += math.sqrt(ax * ax + ay * ay + az * az)

    def rate(self):
        if not self.count or self.last_t == self.first_t:
            return 0.0
        return (self.count - 1) / (self.last_t - self.first_t)


def phase_measure(args, p_sock, i_sock):
    p = Phase()
    pts = StreamStats("points", 1, 1380, 56300, 96)
    imu = StreamStats("imu", 0, 60, 56400, 1)
    deadline = time.monotonic() + args.duration
    while time.monotonic() < deadline:
        r, _, _ = select.select([p_sock, i_sock], [], [], 0.5)
        for s in r:
            buf, addr = s.recvfrom(4096)
            (pts if s is p_sock else imu).feed(buf, addr, args.lidar_ip)

    tol = args.tolerance
    for st, want_rate in [(pts, 2083.33), (imu, 200.0)]:
        rate = st.rate()
        print(f"  {st.name}: {st.count} pkts, {rate:.1f}/s, udp_cnt gaps {st.gaps}")
        p.check(st.count > 0, f"{st.name}: no packets received")
        if st.count:
            p.check(abs(rate - want_rate) <= want_rate * tol,
                    f"{st.name}: rate {rate:.1f}/s outside {want_rate}±{tol * 100:.0f}%")
            p.check(st.gaps == 0, f"{st.name}: {st.gaps} udp_cnt gaps")
            for b in st.bad:
                p.check(False, f"{st.name}: {b}")
    if imu.count:
        mean_g = imu.acc_norm_sum / imu.count
        p.check(0.9 <= mean_g <= 1.1, f"imu |acc| mean {mean_g:.3f} g outside 0.9-1.1")
        print(f"  imu |acc| mean = {mean_g:.3f} g")
    return p


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--host-ip", default="192.168.77.50")
    ap.add_argument("--lidar-ip", default="192.168.77.12")
    ap.add_argument("--duration", type=float, default=10.0)
    ap.add_argument("--tolerance", type=float, default=0.02)
    ap.add_argument("--attempts", type=int, default=5)
    ap.add_argument("--skip-measure", action="store_true")
    ap.add_argument("--unicast-discovery", action="store_true",
                    help="send 0x0000 unicast instead of broadcast (debug aid; "
                         "the real SDK only broadcasts)")
    args = ap.parse_args()

    seq_counter = iter(range(1, 1 << 30))
    results = []

    # Bind data listeners BEFORE configuring so no packets are missed.
    cmd_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    cmd_sock.bind((args.host_ip, 56101))
    push_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    push_sock.bind((args.host_ip, 56201))
    p_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    p_sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 8 << 20)
    p_sock.bind((args.host_ip, 56301))
    i_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    i_sock.bind((args.host_ip, 56401))

    def run(name, phase):
        ok = not phase.failures
        print(f"[{'PASS' if ok else 'FAIL'}] {name}")
        for f in phase.failures:
            print(f"       - {f}")
        results.append(ok)

    print("== phase 1: discover ==")
    ph, _ = phase_discover(args, next(seq_counter))
    run("discover", ph)
    if ph.failures:
        sys.exit(1)

    print("== phase 2: inquire ==")
    run("inquire", phase_inquire(args, cmd_sock, next(seq_counter)))

    print("== phase 3: configure ==")
    run("configure", phase_configure(args, cmd_sock, seq_counter))

    if not args.skip_measure:
        print(f"== phase 4: measure ({args.duration:.0f}s) ==")
        run("measure", phase_measure(args, p_sock, i_sock))

        # Optional: note whether the 1 Hz state push arrived (not a failure).
        r, _, _ = select.select([push_sock], [], [], 2.0)
        if r:
            buf, addr = push_sock.recvfrom(4096)
            frame, err = parse_ctrl(buf)
            ok = frame and frame["cmd_id"] == 0x0102 and addr == (args.lidar_ip, 56200)
            print(f"  state push (0x0102) observed from {addr}: {'ok' if ok else err}")
        else:
            print("  note: no 0x0102 state push observed (optional feature)")

    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
