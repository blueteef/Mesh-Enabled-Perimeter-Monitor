# Mesh protocol (DRAFT - not implemented)

> **Status: design proposal.** Nothing on this page exists in firmware yet. The sensors, events and settings it depends on
> do exist and are verified standalone. Expect this to change once we test against a real radio.

## Goals

1. A phone or any Meshtastic client can read alerts **without special software**.
2. Messages are tiny. LoRa airtime is scarce and shared, so every byte costs everyone on the mesh.
3. A flapping sensor must not flood the mesh. Alerts are rate-limited per type (`mesh.alert_holdoff_s`).
4. Where possible, use Meshtastic's native data types so apps already know how to show them (position, environment).

## Physical link

- RP2040 UART1 (GP4 TX, GP5 RX) to the Meshtastic radio's serial pins, 3.3 V, **38400 baud** (`mesh.baud`), 8N1.
- Meshtastic's **Serial module** is configured on the radio. Pin numbers on the radio side depend on the board (RAK4631
  and XIAO nRF52840 differ, and Meshtastic's pin numbering can differ from the silkscreen). **To be verified on the bench.**

## Two layers, built in this order

### Layer 1 - Text alerts (Serial module, TEXTMSG mode)

The radio forwards whatever text the node writes to the serial port as a mesh text message. Anyone with a phone sees it.
Compact, human-readable, still machine-parseable:

```
<node> <TYPE> <fields>
```

| Message | Example | Meaning |
|---|---|---|
| Detection | `N1 DET B 240cm e80` | Node N1, radar sector B, target 240 cm, energy 80 |
| Clear | `N1 CLR B` | Sector B is clear again |
| Tamper | `N1 TMP TILT 41` / `N1 TMP SHAKE` | Tilt angle in degrees / shaken |
| Heartbeat | `N1 HB 12h 3.9V 21C 853hPa fix7` | Uptime, battery voltage (when measured), temperature, pressure, GPS satellites |
| GPS fix | `N1 GPS 12.3456,-65.4321` | Only on request or fix change (privacy: see below) |

Rules:
- Each message is well under 60 bytes (Meshtastic allows more, but short messages fit more reliably).
- `node.name` (up to 15 chars, short is better) is the node label.
- Detections are rate-limited: at most one per sector per `mesh.alert_holdoff_s`, with a follow-up `CLR`.
- Heartbeats are periodic (`mesh.heartbeat_s`, 0 = off).

### Layer 2 - Native packets (Serial module, PROTO mode)

Meshtastic's PROTO serial mode speaks the same protobuf API as phones. That lets the node send:

- **Position** packets, so the node shows up on the Meshtastic map.
- **Environment telemetry** (temperature, pressure, and humidity once a BME280 is fitted) that apps already graph.
- A private application port for detection events, for a future dashboard.

Known caution from the Meshtastic community: PROTO-to-PROTO serial setups have been reported not to work; PROTO to TEXTMSG worked.
We will test this rather than assume. Layer 1 stays as the fallback and works with any phone.

## Inbound (mesh to node)

Planned, text commands from a trusted node/phone: `N1 ARM`, `N1 DISARM`, `N1 STATUS`. Needs authentication thought
(channel key is the minimum) before it is enabled; off by default.

## Privacy and safety

- Precise coordinates over a mesh are visible to everyone with the channel key. Position sharing will be a setting, off by default.
- Alerts are **advisory**. This is a hobby early-warning aid, not a safety or security guarantee.

## Open questions to settle on hardware

1. Exact serial pins and numbering on the XIAO nRF52840 and the RAK board.
2. Whether PROTO mode works on the target Meshtastic firmware version.
3. Real airtime budget with several nodes and the chosen LoRa preset.
4. Whether battery voltage measurement (GP26 divider) is fitted in the first hardware spin.
5. Command authentication.
