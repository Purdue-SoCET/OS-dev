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

# Parameters for CRC
CRC32_POLY = 0x04C11DB7  # polynomial
CRC32_INIT = 0xFFFFFFFF  # initial crc register
CRC32_XOUT = 0xFFFFFFFF  # values for XOR at the end
CRC32_REFIN = 1         
CRC32_REFOUT = 1      

# flip byte
def _reverse_u8(x: int) -> int:
    x = ((x & 0xF0) >> 4) | ((x & 0x0F) << 4)
    x = ((x & 0xCC) >> 2) | ((x & 0x33) << 2)
    x = ((x & 0xAA) >> 1) | ((x & 0x55) << 1)
    return x

# flip final 32 bits output
def _reverse_u32(x: int) -> int:
    x = ((x & 0xFFFF0000) >> 16) | ((x & 0x0000FFFF) << 16)
    x = ((x & 0xFF00FF00) >> 8)  | ((x & 0x00FF00FF) << 8)
    return x

# crc calculations based on the crc.sv module
def crc32_calc (data: bytes,
                polynomial: int = CRC32_POLY,
                init: int = CRC32_INIT,
                reflect_in: int = CRC32_REFIN,
                reflect_out: int = CRC32_REFOUT,
                xor_out: int = CRC32_XOUT
                ) -> int:
    crc = init & 0xFFFFFFFF
    for b in data:
        if reflect_in:
            b = _reverse_u8(b)
        crc ^= (b << 24) & 0xFFFFFFFF
        for _ in range(8):
            if (crc & 0x80000000) != 0:
                crc = ((crc << 1) ^ polynomial) & 0xFFFFFFFF
            else:
                crc = (crc << 1) & 0xFFFFFFFF
    if reflect_out:
        crc = _reverse_u32(crc)
    crc ^= xor_out
    return crc & 0xFFFFFFFF

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
    print(f"[META] fid=0x{file_id:08x} size={len(data)} chunk={chunk} name={name_bytes.decode(errors='ignore')}")

    # DATA 
    off, seq, total = 0, 0, len(data)
    t0 = time.time()
    while off < total:
        pt = data[off: off + chunk]

        # attached crc
        header = struct.pack("<BIIH", TYPE_DATA, file_id, seq, len(pt))
        frame_wo_crc = header + pt  
        crc_val = crc32_calc(frame_wo_crc)
        
        # 32bis (4byte at the end of the frame)
        frame_with_crc = frame_wo_crc + struct.pack("<I", crc_val)

        # send frame after slip encoding, crc included frame
        ser.write(slip_encode(frame_with_crc))

        off += len(pt); 
        seq += 1

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
    ap.add_argument("--ifsleep", type=float, default=0.0, help="Sleep seconds between frames")
    args = ap.parse_args()

    rc = send_file(args.port, args.baud, args.file, args.chunk, args.ifsleep)
    raise SystemExit(rc)
