# Heat Pump Dashboard

Web UI for monitoring and setting cabin/battery targets on the STM32 heat pump controller.

## Architecture

```
Browser (Vite/React)  ←WebSocket→  bridge/server.py  ←UART 115200→  STM32 (link.c)
```

- **Telemetry** (5 Hz): `{"t":"tel", ...}` — temperatures in 0.1 °C, pressures in 0.1 bar
- **Setpoints**: `{"t":"set","cbl":22,"cbr":21,"bset":25,"cfan":50,"rfan":30,"mode":1}`

Enable **Dashboard control mode** (`mode: 1`) so setpoints drive HVAC demands instead of bench GPIO.

## Quick start (mock data, no hardware)

```bash
cd dashboard
pip install -r bridge/requirements.txt   # once: websockets, pyserial
./scripts/start-mock.sh                  # bridge + UI
```

Or two terminals:

```bash
# Terminal 1 — bridge
cd dashboard/bridge && pip install -r requirements.txt && python server.py --mock

# Terminal 2 — UI
cd dashboard && npm install && npm run dev
```

Open http://localhost:5173

## With hardware

1. Flash firmware (USART1 on PA9/PA10 @ 115200).
2. Connect USB‑serial to the debug header.
3. Run bridge: `python server.py --port /dev/cu.usbmodemXXXX` (macOS) or `COM3` (Windows).
4. Run `npm run dev` as above.

## STM32CubeIDE

After pulling firmware changes, rebuild the project. New sources: `link.c`, `usart.c`, `fan.c`, `setpoint.c`.  
If Cube regenerates makefiles, add those files and `stm32g4xx_hal_uart.c` to the build.

## Next steps

- Closed-loop control from cabin duct temps (reserved ADC pins)
- Smarter setpoint → COP / octovalve / EXV optimization
- CAN passthrough when connected to a vehicle bus
