#!/usr/bin/env python3
"""Probe a BZFS server for its world hash + full world database.

connect -> send "BZFLAG\\r\\n\\r\\n" -> expect 8-byte version + 1-byte id
-> send 4-byte MsgWantWHash ('wh') request -> expect optional MsgCacheURL
('cu') then MsgWantWHash reply with digest -> send MsgGetWorld ('gw')
requests until bytesLeft==0 -> verify MD5 -> report size.
"""
import socket, struct, sys, time, hashlib

host, port = sys.argv[1], int(sys.argv[2])
MsgWantWHash = 0x7768
MsgCacheURL = 0x6375
MsgGetWorld = 0x6777
MsgSuperKill = 0x736b
MsgReject = 0x726a

s = socket.create_connection((host, port), timeout=5)
s.settimeout(5)
s.sendall(b"BZFLAG\r\n\r\n")
hdr = b""
while len(hdr) < 9:
    chunk = s.recv(9 - len(hdr))
    if not chunk:
        break
    hdr += chunk
version, myid = hdr[:8], hdr[8]
print("version:", version, "id byte:", myid)
if not version.startswith(b"BZFS"):
    sys.exit("unexpected version string")

s.sendall(struct.pack("!HH", 0, MsgWantWHash))

buf = b""
cache_url = None
digest = None
end = time.time() + 5
while time.time() < end:
    try:
        chunk = s.recv(4096)
    except socket.timeout:
        break
    if not chunk:
        break
    buf += chunk
    while len(buf) >= 4:
        ln, code = struct.unpack("!HH", buf[:4])
        if len(buf) < 4 + ln:
            break
        body = buf[4:4 + ln]
        buf = buf[4 + ln:]
        if code == MsgCacheURL:
            cache_url = body.split(b"\x00")[0].decode()
            print("cacheURL:", cache_url)
        elif code == MsgWantWHash:
            digest = body.split(b"\x00")[0].decode()
            print("digest:", digest)
            break
    if digest:
        break

if not digest:
    sys.exit("no world hash received")

# pull the world database in chunks
world = b""
ptr = 0
end = time.time() + 30
chunks = 0
while time.time() < end:
    s.sendall(struct.pack("!HHI", 4, MsgGetWorld, ptr))
    # read packets until we get a MsgGetWorld reply
    while True:
        while len(buf) >= 4:
            ln, code = struct.unpack("!HH", buf[:4])
            if len(buf) < 4 + ln:
                break
            body = buf[4:4 + ln]
            buf = buf[4 + ln:]
            if code == MsgGetWorld:
                left = struct.unpack("!I", body[:4])[0]
                world += body[4:]
                ptr = len(world)
                chunks += 1
                if left == 0:
                    print("world complete: %d bytes in %d chunks" % (len(world), chunks))
                    md5 = hashlib.md5(world).hexdigest()
                    ok = (md5 == digest[1:])
                    print("md5 %s (expected %s) -> %s" % (md5, digest[1:], "OK" if ok else "MISMATCH"))
                    sys.exit(0 if ok else 1)
                break
        try:
            chunk = s.recv(65536)
        except socket.timeout:
            print("timeout waiting for chunk; have %d bytes" % len(world))
            sys.exit(1)
        if not chunk:
            print("server closed; have %d bytes" % len(world))
            sys.exit(1)
        buf += chunk

sys.exit("world download did not finish in 30s")