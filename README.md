# Heatpump

STM32G474 heat pump / thermal controller firmware (Tesla Model Y supermanifold inspired), ported from [Superman-Firmware](https://github.com/Wim426F/Superman-Firmware).

## Dashboard

A web UI for live monitoring and setpoints lives in [`dashboard/`](dashboard/README.md):

- EXV positions, octovalve, pressures, temperatures, compressor, pumps
- Set cabin L/R temperature, battery target, radiator fan, cabin fan (stored for future blower)

```bash
# Terminal 1
cd dashboard/bridge && pip install -r requirements.txt && python server.py --mock

# Terminal 2
cd dashboard && npm install && npm run dev
```

Open http://localhost:5173

With hardware: flash firmware, connect USART1 (115200) to USB-serial, run `python server.py --port /dev/cu.usbmodem…`.

## Firmware build

STM32CubeIDE: open project → **Clean** → **Build All**. See **[docs/CUBEIDE_REBUILD.md](docs/CUBEIDE_REBUILD.md)** for step-by-step regeneration and CubeMX USART setup.

New modules: `link.c`, `usart.c`, `fan.c`, `setpoint.c` — USART JSON protocol on PA9/PA10 @ 115200.
