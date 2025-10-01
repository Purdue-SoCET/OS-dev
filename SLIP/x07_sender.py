import os, struct, serial, time
END, ESC, ESC_END, ESC_ESC = 0xC0, 0xDB, 0xDC, 0xDD
TYPE_META, TYPE_DATA, TYPE_ACK = 0x01, 0x02, 0x10

# ESC: Start of the frame

# payload: real data in a packet
# slip_encode: Framing the payload, allowing to tranfser
# Since UART can transfer only serial byte stream. Therefore, we have to
# use specific flag to indicate the start and end of the real by in the packet

def slip_encode (payload: bytes) -> bytes:
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

def slip_read_frame(ser, timeout=2.0) -> bytes | None:
    ser.timeout = 0.1
    start = time.time()
    buf, in_frame, esc = bytearray(), False, False
    while time.time() - start < timeout:
        b = ser.read(1)
        if not b: continue
        b = b[0]
        if b == END:
            if in_frame and buf:
                return bytes(buf)
            buf.clear(); in_frame = True; esc = False; continue
        if not in_frame: continue
        if b == ESC:
            esc = True
            continue
        if esc:
            b = END if b == ESC_END else ESC if b == ESC_ESC else b
            esc = False
        buf.append(b)
    return None

# retries = # of retries for timeout

def send_file(port: str, baud: int, path: str, chunk: int = 1024, retries: int = 2, ack_timeout: float = 5.0):
    # 1) Open serial
    ser = serial.Serial(port, baudrate=baud, bytesize=8, parity="N", stopbits=1, timeout=0.1)
    ser.reset_input_buffer(); ser.reset_output_buffer()
    time.sleep(0.1)

    # 2) Load file
    data = open(path, "rb").read()
    name_bytes = os.path.basename(path).encode("utf-8")[:255]   # fname_len = 1B
    file_id = int.from_bytes(os.urandom(4), "little")
    chunk = max(64, min(chunk, 4096))                           # safe bounds

    # 3) Send META
    meta = struct.pack("<BBIIHB", TYPE_META, 1, file_id, len(data), chunk, len(name_bytes)) + name_bytes
    ser.write(slip_encode(meta))
    print(f"[META] fid=0x{file_id:08x} size={len(data)} chunk={chunk} name={name_bytes.decode(errors='ignore')}")

                    # === Transmission Steps ===
    # 1. Pick p_chunk, then wrap header+pt with SLIP and send it.
    # 2. Receiver stores the data and returns an ACK (next = seq+1).
    # 3. Sender SLIP-decodes the ACK and verifies it.
    # 4. If the ACK is valid, increment off/seq → proceed to the next chunk.
    # 5. If no ACK arrives or it’s invalid, retransmit the same chunk.
        
    # 4) DATA loop (with retry)
    off, seq, total = 0, 0, len(data)
    while off < total:
        pt = data[off: off + chunk]
        header = struct.pack("<BIIH", TYPE_DATA, file_id, seq, len(pt))
        frame  = slip_encode(header + pt)

        tries, ok = 0, False
        while tries <= retries and not ok:
            # 1. send
            ser.write(frame)

            # 2. wait ACK
            fr = slip_read_frame(ser, timeout=ack_timeout)
            if fr and len(fr) >= 9 and fr[0] == TYPE_ACK:
                ack_file, next_seq = struct.unpack("<II", fr[1:9])
                ok = (ack_file == file_id and next_seq == seq + 1)

            if not ok:
                tries += 1
                if tries <= retries:
                    print(f"\r[WARN] seq={seq} no/invalid ACK -> retry {tries}/{retries}", end="", flush=True)
                    # optional tiny backoff
                    time.sleep(0.05)

        if not ok:
            print(f"\n[ERROR] transfer failed at seq={seq}")
            ser.close()
            return 2

        # 3. ACK valid → advance
        off += len(pt); seq += 1
        print(f"\r[DATA] {off}/{total} bytes sent", end="", flush=True)

    print("\n[DONE] transfer complete.")
    ser.close()
    return 0
    
if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser(description="SLIP Sender")
    ap.add_argument("--port", required=True, help="Serial port (e.g. /dev/ttys019)")
    ap.add_argument("--baud", type=int, default=115200, help="Baud rate")
    ap.add_argument("--file", required=True, help="File path to send")
    ap.add_argument("--chunk", type=int, default=1024, help="Chunk size")
    ap.add_argument("--retries", type=int, default=2, help="Retries per chunk")
    ap.add_argument("--timeout", type=float, default=5.0, help="ACK wait timeout")
    args = ap.parse_args()

    rc = send_file(args.port, args.baud, args.file, args.chunk, args.retries, args.timeout)
    raise SystemExit(rc)

            