import struct

PACKET_SIZE = 12
MAGIC0 = 0xA5
MAGIC1 = 0x5A


def encode_packet(sequence, class_index, timestamp_ms, valid):
    if not 0 <= sequence <= 65535:
        raise ValueError("Sequence number out of range")

    if not 0 <= class_index <= 65535:
        raise ValueError("Class index out of range")

    if not 0 <= timestamp_ms <= 0xFFFFFFFF:
        raise ValueError("Timestamp out of range")

    if valid not in (0, 1):
        raise ValueError("Validity flag must be 0 or 1")

    payload = struct.pack(
        "<BBHHIB",
        MAGIC0,
        MAGIC1,
        sequence,
        class_index,
        timestamp_ms,
        valid,
    )

    checksum = 0

    for byte in payload:
        checksum ^= byte

    packet = payload + bytes([checksum])

    assert len(packet) == PACKET_SIZE

    return packet


if __name__ == "__main__":
    print("=== LINUX PACKET ENCODER TEST ===")

    packet = encode_packet(
        sequence=1,
        class_index=892,
        timestamp_ms=123456,
        valid=1,
    )

    print("Packet size:", len(packet), "bytes")
    print("Packet HEX:", packet.hex(" "))

    checksum = 0

    for byte in packet:
        checksum ^= byte

    print("Checksum verification:", "PASS" if checksum == 0 else "FAIL")

    assert packet[:2] == b"\xA5\x5A"
    assert checksum == 0

    print("PACKET ENCODER TEST: PASS")
