import { useCallback, useEffect, useRef, useState } from "react";
import {
  parseTelemetry,
  type SetpointCommand,
  type Telemetry,
  type TelemetryRaw,
} from "./types";

const WS_URL =
  import.meta.env.VITE_WS_URL ??
  `${location.protocol === "https:" ? "wss:" : "ws:"}//${location.hostname}:8765`;

export interface Setpoints {
  cabinL: number;
  cabinR: number;
  battery: number;
  cabinFan: number;
  radiatorFan: number;
  dashboardMode: boolean;
}

export const DEFAULT_SETPOINTS: Setpoints = {
  cabinL: 21,
  cabinR: 21,
  battery: 25,
  cabinFan: 0,
  radiatorFan: 30,
  dashboardMode: true,
};

export function useTelemetry() {
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [connected, setConnected] = useState(false);
  const [setpoints, setSetpoints] = useState<Setpoints>(DEFAULT_SETPOINTS);
  const wsRef = useRef<WebSocket | null>(null);

  const sendSet = useCallback((partial?: Partial<Setpoints>) => {
    const sp = { ...setpoints, ...partial };
    const msg: SetpointCommand = {
      t: "set",
      cbl: sp.cabinL,
      cbr: sp.cabinR,
      bset: sp.battery,
      cfan: sp.cabinFan,
      rfan: sp.radiatorFan,
      mode: sp.dashboardMode ? 1 : 0,
    };
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(msg));
    }
  }, [setpoints]);

  useEffect(() => {
    let cancelled = false;
    let retryTimer: ReturnType<typeof setTimeout>;

    const connect = () => {
      const ws = new WebSocket(WS_URL);
      wsRef.current = ws;

      ws.onopen = () => {
        if (!cancelled) setConnected(true);
      };

      ws.onclose = () => {
        if (!cancelled) {
          setConnected(false);
          retryTimer = setTimeout(connect, 2000);
        }
      };

      ws.onerror = () => ws.close();

      ws.onmessage = (ev) => {
        try {
          const raw = JSON.parse(ev.data as string) as TelemetryRaw;
          if (raw.t !== "tel") return;
          const tel = parseTelemetry(raw);
          if (!cancelled) {
            setTelemetry(tel);
          }
        } catch {
          /* ignore malformed */
        }
      };
    };

    connect();
    return () => {
      cancelled = true;
      clearTimeout(retryTimer);
      wsRef.current?.close();
    };
  }, []);

  return {
    telemetry,
    connected,
    setpoints,
    setSetpoints,
    sendSet,
  };
}
