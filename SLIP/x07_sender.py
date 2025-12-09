# x07_send_no_ack.py
import os, struct, serial, time

# Special Byte
END = 0xC0
ESC = 0xDB
ESC_END = 0xDC
ESC_ESC = 0xDD

# Frame type
TYPE_META = 0x01
TYPE_DATA = 0x02

# First frame flag
is_first_frame = 1

def wait_ack():
    buf = bytearray()
    while True# x07_send_no_ack.py
import os, struct, serial, time

# Special Byte
END = 0xC0
ESC = 0xDB
ESC_END = 0xDC
ESC_ESC = 0xDD

# Frame type
TYPE_META = 0x01
TYPE_DATA = 0x02

def wait_ack(ser):
    buf = bytearray()
    while True:
        tmp_buf = ser.read(1)
        if not tmp_buf:
            continue
        buf += tmp_buf
        if len(buf) > 3:
            buf = buf[-3:]
        if buf == b"ACK":
            return

def slip_encode(payload: bytes) -> bytes:
    out = bytearray([END])
    for b in payload:
        if b == END:
            out += bytes([ESC, ESC_END])
        elif b == ESC:
            out += bytes([ESC, ESC_ESC])
        else:
            out.append(b)
    out.append(END)
    return bytes(out)

def send_file(port: str, baud: int, path: str, chunk: int = 1024, inter_frame_sleep: float = 0.0):
    ser = serial.Serial(port, baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=0.1)
    ser.reset_input_buffer(); 
    ser.reset_output_buffer()
    time.sleep(0.1)

    data = open(path, "rb").read()
    name_bytes = os.path.basename(path).encode("utf-8")[:255]
    file_id = int.from_bytes(os.urandom(4), "little")
    chunk = max(64, min(chunk, 4096))

    # META
    meta = struct.pack("<BBIIHB", TYPE_META, 1, file_id, len(data), chunk, len(name_bytes)) + name_bytes
    ser.write(slip_encode(meta))
    wait_ack(ser)
    print(f"[META] fid=0x{file_id:08x} size={len(data)} chunk={chunk} name={name_bytes.decode(errors='ignore')}")

    # DATA 
    off, seq, total = 0, 0, len(data)
    t0 = time.time()
    while off < total:
        pt = data[off: off + chunk]
        header = struct.pack("<BIIH", TYPE_DATA, file_id, seq, len(pt))
        ser.write(slip_encode(header + pt))
        wait_ack(ser)
        off += len(pt); seq += 1

        if inter_frame_sleep > 0:
            time.sleep(inter_frame_sleep)

        if seq % 16 == 0 or off == total:
            print(f"\r[DATA] {off}/{total} bytes sent", end="", flush=True)

    dt = time.time() - t0
    print(f"\n[DONE] {total} bytes in {dt:.3f}s ({(total/1024.0)/max(dt,1e-9):.1f} KB/s, frames={seq})")
    ser.close()
    return 0

if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="SLIP Sender (no ACK)")
    ap.add_argument("--port", required=True, help="Serial port (e.g. /dev/ttys019)")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate")
    ap.add_argument("--file", required=True, help="File path to send")
    ap.add_argument("--chunk", type=int, default=1024, help="Chunk size (64..4096)")
    # ap.add_argument("—-ifsleep", type=float, default=0.0, help="Sleep seconds between frames")
    args = ap.parse_args()

    # rc = send_file(args.port, args.baud, args.file, args.chunk, args.ifsleep)
    rc = send_file(args.port, args.baud, args.file, args.chunk)
    raise SystemExit(rc):
        tmp_buf = ser.read(1)
        if not tmp_buf:
            continue
        buf += tmp_buf
        if len(buf) > 4:
            buf = buf[-4:]
        if buf == b"ACK":
            return
        if buf == b"END":
            is_first_frame = 1
            return

def slip_encode(payload: bytes) -> bytes:
    out = bytearray([END])
    for b in payload:
        if b == END:
            out += bytes([ESC, ESC_END])
        elif b == ESC:
            out += bytes([ESC, ESC_ESC])
        else:
            out.append(b)
    out.append(END)
    return bytes(out)

def send_file(port: str, baud: int, path: str, chunk: int = 1024, inter_frame_sleep: float = 0.0):
    ser = serial.Serial(port, baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=0.1)
    ser.reset_input_buffer(); 
    ser.reset_output_buffer()
    time.sleep(0.1)

    data = open(path, "rb").read()
    name_bytes = os.path.basename(path).encode("utf-8")[:255]
    file_id = int.from_bytes(os.urandom(4), "little")
    chunk = max(64, min(chunk, 4096))

    # META
    meta = struct.pack("<BBIIHB", TYPE_META, 1, file_id, len(data), chunk, len(name_bytes)) + name_bytes
    if (is_first_frame):
        ser.write(slip_encode(meta))
        is_first_frame = 0
    else:
        ser.write(slip_encode(meta))
        wait_ack(ser)
    print(f"[META] fid=0x{file_id:08x} size={len(data)} chunk={chunk} name={name_bytes.decode(errors='ignore')}")

    # DATA 
    off, seq, total = 0, 0, len(data)
    t0 = time.time()
    while off < total:
        pt = data[off: off + chunk]
        header = struct.pack("<BIIH", TYPE_DATA, file_id, seq, len(pt))
        ser.write(slip_encode(header + pt))
        wait_ack(ser)
        off += len(pt); seq += 1

        if inter_frame_sleep > 0:
            time.sleep(inter_frame_sleep)

        if seq % 16 == 0 or off == total:
            print(f"\r[DATA] {off}/{total} bytes sent", end="", flush=True)

    dt = time.time() - t0
    print(f"\n[DONE] {total} bytes in {dt:.3f}s ({(total/1024.0)/max(dt,1e-9):.1f} KB/s, frames={seq})")
    ser.close()
    return 0

if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="SLIP Sender (no ACK)")
    ap.add_argument("--port", required=True, help="Serial port (e.g. /dev/ttys019)")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate")
    ap.add_argument("--file", required=True, help="File path to send")
    ap.add_argument("--chunk", type=int, default=1024, help="Chunk size (64..4096)")
    ap.add_argument("—ifsleep", type=float, default=0.0, help="Sleep seconds between frames")
    args = ap.parse_args()

    rc = send_file(args.port, args.baud, args.file, args.chunk, args.ifsleep)
    raise SystemExit(rc)