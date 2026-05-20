import { useTelemetry, DEFAULT_SETPOINTS } from "./useTelemetry";
import { HTM_NAMES, OCTO_NAMES } from "./types";

function Metric({
  label,
  value,
  unit,
}: {
  label: string;
  value: string;
  unit?: string;
}) {
  return (
    <div className="panel">
      <h2>{label}</h2>
      <div className="metric">
        {value}
        {unit && <small> {unit}</small>}
      </div>
    </div>
  );
}

function DemandBits({ mask }: { mask: number }) {
  const items = [
    { bit: 0, label: "Cabin cool", cool: true },
    { bit: 1, label: "Batt cool", cool: true },
    { bit: 2, label: "PT cool", cool: true },
    { bit: 3, label: "Cabin L heat", heat: true },
    { bit: 4, label: "Cabin R heat", heat: true },
    { bit: 5, label: "Batt heat", heat: true },
  ];
  return (
    <div className="demand-bits">
      {items.map(({ bit, label, heat, cool }) => {
        const on = (mask & (1 << bit)) !== 0;
        return (
          <span
            key={bit}
            className={`bit ${on ? "on" : ""} ${on && heat ? "heat" : ""} ${on && cool ? "cool" : ""}`}
          >
            {label}
          </span>
        );
      })}
    </div>
  );
}

export default function App() {
  const { telemetry: t, connected, setpoints, setSetpoints, sendSet } =
    useTelemetry();

  const fmt = (v: number | undefined, d = 1) =>
    v !== undefined ? v.toFixed(d) : "—";

  const exvLabels = ["LCC", "CC-L", "Recirc", "CC-R", "Evap", "Chiller"];
  const exvVals = t
    ? [t.exv.lcc, t.exv.ccL, t.exv.recirc, t.exv.ccR, t.exv.evap, t.exv.chiller]
    : [0, 0, 0, 0, 0, 0];

  return (
    <div className="app">
      <header className="header">
        <div>
          <h1>Heat Pump Control</h1>
          <p className="hint" style={{ margin: 0 }}>
            Monitor refrigerant & coolant loop · set cabin & battery targets
          </p>
        </div>
        <span className="status-pill">
          <span className={`status-dot ${connected ? "live" : ""}`} />
          {connected ? "Live" : "Connecting…"}
          {t && (
            <span style={{ color: "var(--muted)", marginLeft: 4 }}>
              · {t.ms} ms
            </span>
          )}
        </span>
      </header>

      <section className="grid grid-3" style={{ marginBottom: "1rem" }}>
        <Metric label="Battery" value={fmt(t?.temps.battery)} unit="°C" />
        <Metric label="Powertrain" value={fmt(t?.temps.powertrain)} unit="°C" />
        <Metric label="Ambient (est.)" value={fmt(t?.temps.ambient)} unit="°C" />
        <Metric
          label="Compressor"
          value={t ? String(t.compressor.rpm) : "—"}
          unit="RPM"
        />
        <Metric
          label="Comp. duty"
          value={fmt(t?.compressor.dutyPct)}
          unit="%"
        />
        <Metric
          label="Comp. power"
          value={t ? String(t.compressor.powerW) : "—"}
          unit="W"
        />
      </section>

      <section className="grid grid-2" style={{ marginBottom: "1rem" }}>
        <div className="panel">
          <h2>Refrigerant pressures</h2>
          <div className="grid grid-3">
            <div>
              <div className="hint">Discharge</div>
              <div className="metric">{fmt(t?.pressures.discharge)} bar</div>
            </div>
            <div>
              <div className="hint">Suction</div>
              <div className="metric">{fmt(t?.pressures.suction)} bar</div>
            </div>
            <div>
              <div className="hint">Subcool line</div>
              <div className="metric">{fmt(t?.pressures.subcool)} bar</div>
            </div>
          </div>
          <div className="grid grid-3" style={{ marginTop: "0.75rem" }}>
            <div>
              <div className="hint">T discharge</div>
              <div className="metric">{fmt(t?.temps.disc)} °C</div>
            </div>
            <div>
              <div className="hint">T suction</div>
              <div className="metric">{fmt(t?.temps.suction)} °C</div>
            </div>
            <div>
              <div className="hint">T subcool</div>
              <div className="metric">{fmt(t?.temps.subcool)} °C</div>
            </div>
          </div>
        </div>

        <div className="panel">
          <h2>Pumps & octovalve</h2>
          <p>
            Battery pump: <strong>{t?.pumps.batteryDuty ?? "—"}%</strong>
            {" · "}
            flow ~{t?.pumps.batteryFlow ?? "—"}
          </p>
          <p>
            PT pump: <strong>{t?.pumps.ptDuty ?? "—"}%</strong>
            {" · "}
            flow ~{t?.pumps.ptFlow ?? "—"}
          </p>
          <p>
            Octovalve:{" "}
            <strong>
              {t ? OCTO_NAMES[t.octovalve.position] ?? t.octovalve.position : "—"}
            </strong>
            {t && t.octovalve.setpoint !== t.octovalve.position && (
              <span className="hint"> → target {t.octovalve.setpoint}</span>
            )}
          </p>
          <p>
            Mode:{" "}
            <strong>
              {t ? HTM_NAMES[t.heatTransferMode] ?? t.heatTransferMode : "—"}
            </strong>
            {" · "}
            LCC SOV: {t?.sovLcc ? "closed" : "open"}
          </p>
          {t && <DemandBits mask={t.thermalDemands} />}
        </div>
      </section>

      <section className="panel" style={{ marginBottom: "1rem" }}>
        <h2>Expansion valves (0–255)</h2>
        <div className="exv-row">
          {exvLabels.map((name, i) => (
            <div key={name} className="exv-bar-wrap">
              <label>{name}</label>
              <div className="exv-bar">
                <div
                  className="exv-fill"
                  style={{ height: `${(exvVals[i] / 255) * 100}%` }}
                />
              </div>
              <div className="exv-val">{exvVals[i]}</div>
            </div>
          ))}
        </div>
      </section>

      <section className="panel">
        <h2>Setpoints</h2>
        <label className="toggle">
          <input
            type="checkbox"
            checked={setpoints.dashboardMode}
            onChange={(e) =>
              setSetpoints((s) => ({ ...s, dashboardMode: e.target.checked }))
            }
          />
          Dashboard control mode (disable GPIO bench inputs)
        </label>

        {(
          [
            ["Cabin left", "cabinL", 16, 28],
            ["Cabin right", "cabinR", 16, 28],
            ["Battery target", "battery", 10, 45],
            ["Cabin fan", "cabinFan", 0, 100],
            ["Radiator fan", "radiatorFan", 0, 100],
          ] as const
        ).map(([label, key, min, max]) => (
          <div className="form-row" key={key}>
            <label>
              {label}: {setpoints[key]} {key.includes("Fan") ? "%" : "°C"}
            </label>
            <input
              type="range"
              min={min}
              max={max}
              value={setpoints[key]}
              onChange={(e) =>
                setSetpoints((s) => ({
                  ...s,
                  [key]: Number(e.target.value),
                }))
              }
            />
          </div>
        ))}

        <div className="actions">
          <button
            type="button"
            className="btn btn-primary"
            onClick={() => sendSet()}
          >
            Apply setpoints
          </button>
          <button
            type="button"
            className="btn btn-ghost"
            onClick={() => setSetpoints({ ...DEFAULT_SETPOINTS })}
          >
            Reset defaults
          </button>
        </div>
        <p className="hint">
          Cabin fan is stored for future HVAC blower control; radiator fan drives
          TIM2 on PB11. Cabin heat/cool requests are derived from setpoints until
          duct temperature sensors are added.
        </p>
      </section>
    </div>
  );
}
