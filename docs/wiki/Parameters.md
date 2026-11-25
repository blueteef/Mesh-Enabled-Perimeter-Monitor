# Parameter reference

> **Generated file** - do not edit by hand. Run `python tools/gen_param_docs.py` after changing
> the table in `src/config_store.cpp`.

48 parameters. All of them can be changed live from the CLI (`set <name> <value>`) or the
web app, and persist across power cycles once you `save`. Parameters marked **reboot** take
effect only after `save` then `reboot`.

## Node identity

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `node.name` | str | `NODE1` | up to 15 chars |  | Short node label sent in mesh messages |

## System / status LED / USB stream

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `sys.led_enable` | bool | `1` | - |  | Status LED on/off |
| `sys.led_brightness` | u32 | `20` | 0 to 255 |  | Status LED brightness |
| `sys.stream_hz` | u32 | `5` | 0 to 50 |  | USB telemetry stream rate (0 = off) |

## Meshtastic link (used by the mesh layer, not yet wired in)

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `mesh.baud` | u32 | `38400` | 1200 to 921600 | yes | UART baud to Meshtastic radio |
| `mesh.heartbeat_s` | u32 | `300` | 0 to 86400 |  | Periodic status message interval (0 = off) |
| `mesh.alert_holdoff_s` | u32 | `30` | 0 to 3600 |  | Minimum seconds between alerts of the same type |

## GPS

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `gps.baud` | u32 | `9600` | 1200 to 921600 | yes | UART baud to GPS (Adafruit Ultimate GPS default 9600) |
| `gps.enable` | bool | `1` | - | yes | GPS powered via EN pin at boot |

## Environment sensor (BMP280 / BME280)

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `env.poll_ms` | u32 | `1000` | 100 to 60000 |  | BMP280/BME280 read interval |
| `env.sealevel_inhg` | f32 | `29.92` | 26.5 to 32.5 |  | Sea-level pressure (inHg) for altitude calc |
| `env.temp_offset_f` | f32 | `0` | -36 to 36 |  | Temperature calibration offset (degF) |

## IMU (BMI323)

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `imu.enable` | bool | `1` | - | yes | Enable BMI323 |
| `imu.odr` | u32 | `8` | 1 to 12 | yes | BMI323 ODR code (8 = 100 Hz, 9 = 200 Hz; VERIFY on bench) |
| `imu.acc_range` | u32 | `1` | 0 to 3 | yes | Accel range: 0=2g 1=4g 2=8g 3=16g |
| `imu.gyr_range` | u32 | `2` | 0 to 4 | yes | Gyro range: 0=125 1=250 2=500 3=1000 4=2000 dps |
| `imu.poll_hz` | u32 | `100` | 1 to 400 |  | Firmware IMU read rate |
| `imu.lpf` | f32 | `0.05` | 0.001 to 1 |  | Gravity low-pass coefficient (smaller = smoother) |

## Tamper detection

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `tamper.enable` | bool | `1` | - |  | Tilt/motion tamper detection |
| `tamper.arm_delay_s` | u32 | `60` | 0 to 3600 |  | Auto-arm this long after boot (0 = manual 'arm') |
| `tamper.tilt_deg` | f32 | `15` | 1 to 180 |  | Tilt from armed pose that triggers alarm |
| `tamper.tilt_ms` | u32 | `500` | 0 to 60000 |  | Tilt must persist this long |
| `tamper.motion_mg` | f32 | `150` | 5 to 4000 |  | Vibration/shake envelope threshold (mg) |
| `tamper.gyro_dps` | f32 | `60` | 5 to 2000 |  | Rotation rate threshold |
| `tamper.motion_ms` | u32 | `200` | 0 to 60000 |  | Motion must persist this long |
| `tamper.holdoff_s` | u32 | `60` | 1 to 3600 |  | After an alarm, re-arm at the new pose after this long |

## Magnetometer (QMC5883L) and calibration

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `mag.enable` | bool | `1` | - | yes | Enable QMC5883L |
| `mag.poll_hz` | u32 | `20` | 1 to 50 |  | Mag read rate |
| `mag.declination_deg` | f32 | `0` | -30 to 30 |  | Magnetic declination (east positive) |
| `mag.sx` | i32 | `1` | -1 to 1 |  | Axis sign flip X (mounting) |
| `mag.sy` | i32 | `1` | -1 to 1 |  | Axis sign flip Y (mounting) |
| `mag.sz` | i32 | `1` | -1 to 1 |  | Axis sign flip Z (mounting) |
| `mag.off_x` | f32 | `0` | -40000 to 40000 |  | Hard-iron offset X (set by magcal) |
| `mag.off_y` | f32 | `0` | -40000 to 40000 |  | Hard-iron offset Y (set by magcal) |
| `mag.off_z` | f32 | `0` | -40000 to 40000 |  | Hard-iron offset Z (set by magcal) |
| `mag.scale_x` | f32 | `1` | 0.1 to 10 |  | Soft-iron scale X (set by magcal) |
| `mag.scale_y` | f32 | `1` | 0.1 to 10 |  | Soft-iron scale Y (set by magcal) |
| `mag.scale_z` | f32 | `1` | 0.1 to 10 |  | Soft-iron scale Z (set by magcal) |

## Radar (LD2410C x3)

| Parameter | Type | Default | Range | Reboot | Description |
|---|---|---|---|---|---|
| `radar.baud` | u32 | `256000` | 9600 to 460800 | yes | UART baud to LD2410 modules (factory 256000) |
| `radar.enable_mask` | u32 | `7` | 0 to 7 |  | Bitmask of radars that raise detections (A=1 B=2 C=4) |
| `radar.min_energy` | u32 | `30` | 0 to 100 |  | Minimum target energy to count as detection |
| `radar.max_ft` | f32 | `20` | 1 to 20 |  | Ignore targets farther than this (feet) |
| `radar.hold_ms` | u32 | `2000` | 0 to 60000 |  | Keep 'present' this long after last hit |
| `radar.max_move_gate` | u32 | `8` | 1 to 8 |  | Module max moving gate, 0.75 m each ('radar apply') |
| `radar.max_static_gate` | u32 | `8` | 1 to 8 |  | Module max stationary gate ('radar apply') |
| `radar.nobody_s` | u32 | `5` | 0 to 65535 |  | Module no-one timeout ('radar apply') |
| `radar.sens_move` | u32 | `50` | 0 to 100 |  | Module moving threshold, all gates ('radar apply') |
| `radar.sens_static` | u32 | `40` | 0 to 100 |  | Module stationary threshold, all gates ('radar apply') |
