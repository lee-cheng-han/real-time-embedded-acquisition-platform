"""POSIX serial transport for DRATS without third-party dependencies."""

from __future__ import annotations

import os
import select
import termios
import time


class SerialTransportError(OSError):
    pass


class SerialTransport:
    def __init__(self, device: str, baud: int = 2_000_000):
        speed_name = f"B{baud}"
        if not hasattr(termios, speed_name):
            raise SerialTransportError(f"platform does not support baud {baud}")
        self._fd = os.open(device, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        try:
            attributes = termios.tcgetattr(self._fd)
            attributes[0] = 0
            attributes[1] = 0
            attributes[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
            attributes[3] = 0
            attributes[4] = getattr(termios, speed_name)
            attributes[5] = getattr(termios, speed_name)
            attributes[6][termios.VMIN] = 0
            attributes[6][termios.VTIME] = 0
            termios.tcsetattr(self._fd, termios.TCSANOW, attributes)
            termios.tcflush(self._fd, termios.TCIOFLUSH)
        except Exception:
            os.close(self._fd)
            self._fd = -1
            raise

    def close(self) -> None:
        if self._fd >= 0:
            os.close(self._fd)
            self._fd = -1

    def __enter__(self) -> "SerialTransport":
        return self

    def __exit__(self, exc_type, exc, traceback) -> None:
        self.close()

    def write(self, data: bytes, timeout: float = 1.0) -> None:
        deadline = time.monotonic() + timeout
        offset = 0
        while offset < len(data):
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TimeoutError("serial write timed out")
            _, writable, _ = select.select([], [self._fd], [], remaining)
            if not writable:
                raise TimeoutError("serial write timed out")
            offset += os.write(self._fd, data[offset:])

    def read(self, capacity: int = 4096, timeout: float = 0.1) -> bytes:
        readable, _, _ = select.select([self._fd], [], [], timeout)
        if not readable:
            return b""
        return os.read(self._fd, capacity)
