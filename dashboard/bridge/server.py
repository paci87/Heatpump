#!/usr/bin/env python3
"""
WebSocket bridge: connects the heat pump dashboard to the STM32 USART link.

  Mock (no hardware):  python server.py --mock
  Serial:              python server.py --port /dev/cu.usbmodem*

WebSocket: ws://127.0.0.1:8765
"""

from __future__ import annotations

import argparse
import asyncio
import json
import math
import time
from typing import Any, Set

try:
    import serial
except ImportError:
    serial = None  # type: ignore

from websockets.asyncio.server import serve
from websockets.exceptions import ConnectionClosed

WS_HOST = "127.0.0.1"
WS_PORT = 8765


def mock_telemetry(t0: float) -> dict:
    t = time.time() - t0
    wobble = math.sin(t * 0.3)
    return {
        "t": "tel",
        "ms": int(t * 1000) % 100000,
        "tb": int((18 + wobble * 2) * 10),
        "tp": int((42 + wobble) * 10),
        "ta": int((5 + wobble) * 10),
        "p_hi": int((12 + wobble) * 10),
        "p_lo": int((3.2 + wobble * 0.2) * 10),
        "p_sc": int((8 + wobble * 0.5) * 10),
        "t_disc": int((65 + wobble * 3) * 10),
        "t_suct": int((12 + wobble) * 10),
        "t_sc": int((28 + wobble) * 10),
        "crpm": int(2800 + wobble * 400),
        "cdty": int((45 + wobble * 10) * 10),
        "ctemp": 38,
        "cpwr": 1200,
        "pb": 40,
        "pp": 40,
        "fb": 1200,
        "fp": 1100,
        "oct": 3,
        "oct_sp": 3,
        "exv": [200, 255, 0, 255, 80, 120],
        "sov": 0,
        "dem": 8,
        "htm": 1,
        "cbl": 21,
        "cbr": 21,
        "bset": 25,
        "cfan": 0,
        "rfan": 30,
        "mode": 1,
    }


class SerialBridge:
    def __init__(self, port: str, baud: int = 115200) -> None:
        if serial is None:
            raise RuntimeError("pyserial not installed: pip install pyserial")
        self.ser = serial.Serial(port, baud, timeout=0)
        self._buf = ""

    def read_lines(self) -> list[dict]:
        chunk = self.ser.read(512)
        if chunk:
            self._buf += chunk.decode("utf-8", errors="ignore")
        out: list[dict] = []
        while "\n" in self._buf:
            line, self._buf = self._buf.split("\n", 1)
            line = line.strip()
            if not line:
                continue
            try:
                out.append(json.loads(line))
            except json.JSONDecodeError:
                pass
        return out

    def write_set(self, msg: dict) -> None:
        line = json.dumps(msg, separators=(",", ":")) + "\n"
        self.ser.write(line.encode("utf-8"))


async def broadcast(clients: Set[Any], payload: str) -> None:
    dead: Set[Any] = set()
    for ws in clients:
        try:
            await ws.send(payload)
        except ConnectionClosed:
            dead.add(ws)
    clients -= dead


async def relay_mock(clients: Set[Any], t0: float) -> None:
    while True:
        if clients:
            await broadcast(clients, json.dumps(mock_telemetry(t0)))
        await asyncio.sleep(0.2)


async def relay_serial(clients: Set[Any], bridge: SerialBridge) -> None:
    while True:
        for obj in bridge.read_lines():
            if obj.get("t") != "tel":
                continue
            await broadcast(clients, json.dumps(obj))
        await asyncio.sleep(0.02)


async def main() -> None:
    parser = argparse.ArgumentParser(description="Heat pump dashboard bridge")
    parser.add_argument("--mock", action="store_true", help="Simulate telemetry")
    parser.add_argument("--port", help="Serial port")
    parser.add_argument("--baud", type=int, default=115200)
    args = parser.parse_args()

    clients: Set[Any] = set()
    bridge: SerialBridge | None = None
    t0 = time.time()

    if args.mock:
        print("Mock telemetry enabled")
    else:
        if not args.port:
            raise SystemExit("Provide --port or use --mock")
        bridge = SerialBridge(args.port, args.baud)
        print(f"Serial: {args.port} @ {args.baud}")

    async def handler(websocket):
        clients.add(websocket)
        try:
            async for message in websocket:
                if bridge is None:
                    continue
                try:
                    data = json.loads(message)
                except json.JSONDecodeError:
                    continue
                if data.get("t") == "set":
                    bridge.write_set(data)
        finally:
            clients.discard(websocket)

    async with serve(handler, WS_HOST, WS_PORT):
        print(f"WebSocket: ws://{WS_HOST}:{WS_PORT}")
        if args.mock:
            await relay_mock(clients, t0)
        else:
            await relay_serial(clients, bridge)


if __name__ == "__main__":
    asyncio.run(main())
