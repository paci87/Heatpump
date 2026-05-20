/** Raw telemetry from MCU (temperatures/pressures in deci-units unless noted). */
export interface TelemetryRaw {
  t: "tel";
  ms: number;
  tb: number;
  tp: number;
  ta: number;
  p_hi: number;
  p_lo: number;
  p_sc: number;
  t_disc: number;
  t_suct: number;
  t_sc: number;
  crpm: number;
  cdty: number;
  ctemp: number;
  cpwr: number;
  pb: number;
  pp: number;
  fb: number;
  fp: number;
  oct: number;
  oct_sp: number;
  exv: [number, number, number, number, number, number];
  sov: number;
  dem: number;
  htm: number;
  cbl: number;
  cbr: number;
  bset: number;
  cfan: number;
  rfan: number;
  mode: number;
}

export interface Telemetry {
  ms: number;
  temps: {
    battery: number;
    powertrain: number;
    ambient: number;
    disc: number;
    suction: number;
    subcool: number;
  };
  pressures: {
    discharge: number;
    suction: number;
    subcool: number;
  };
  compressor: {
    rpm: number;
    dutyPct: number;
    tempC: number;
    powerW: number;
  };
  pumps: {
    batteryDuty: number;
    ptDuty: number;
    batteryFlow: number;
    ptFlow: number;
  };
  octovalve: { position: number; setpoint: number };
  exv: {
    lcc: number;
    ccL: number;
    recirc: number;
    ccR: number;
    evap: number;
    chiller: number;
  };
  sovLcc: boolean;
  thermalDemands: number;
  heatTransferMode: number;
  setpoints: {
    cabinL: number;
    cabinR: number;
    battery: number;
    cabinFan: number;
    radiatorFan: number;
    mode: number;
  };
}

export interface SetpointCommand {
  t: "set";
  cbl?: number;
  cbr?: number;
  bset?: number;
  cfan?: number;
  rfan?: number;
  mode?: number;
}

export function parseTelemetry(raw: TelemetryRaw): Telemetry {
  const d = 0.1;
  return {
    ms: raw.ms,
    temps: {
      battery: raw.tb * d,
      powertrain: raw.tp * d,
      ambient: raw.ta * d,
      disc: raw.t_disc * d,
      suction: raw.t_suct * d,
      subcool: raw.t_sc * d,
    },
    pressures: {
      discharge: raw.p_hi * d,
      suction: raw.p_lo * d,
      subcool: raw.p_sc * d,
    },
    compressor: {
      rpm: raw.crpm,
      dutyPct: raw.cdty * d,
      tempC: raw.ctemp,
      powerW: raw.cpwr,
    },
    pumps: {
      batteryDuty: raw.pb,
      ptDuty: raw.pp,
      batteryFlow: raw.fb,
      ptFlow: raw.fp,
    },
    octovalve: { position: raw.oct, setpoint: raw.oct_sp },
    exv: {
      lcc: raw.exv[0],
      ccL: raw.exv[1],
      recirc: raw.exv[2],
      ccR: raw.exv[3],
      evap: raw.exv[4],
      chiller: raw.exv[5],
    },
    sovLcc: raw.sov !== 0,
    thermalDemands: raw.dem,
    heatTransferMode: raw.htm,
    setpoints: {
      cabinL: raw.cbl,
      cabinR: raw.cbr,
      battery: raw.bset,
      cabinFan: raw.cfan,
      radiatorFan: raw.rfan,
      mode: raw.mode,
    },
  };
}

export const OCTO_NAMES: Record<number, string> = {
  0: "Unknown",
  1: "Series",
  2: "Series (rad bypass)",
  3: "Parallel",
  4: "Ambient source",
  5: "COP 1",
};

export const HTM_NAMES: Record<number, string> = {
  0: "Passive",
  1: "Heating",
  2: "Cooling",
};
