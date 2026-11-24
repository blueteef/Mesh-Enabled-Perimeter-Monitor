# How it works

## The idea in one paragraph

Imagine a tripwire that works without a wire. A small box sits at the edge of your camp.
Inside it, three tiny radars each watch a third of the circle around the box. If something
moves (or even stands very still) in range, the box notices. It knows where it is (GPS),
which way it is facing (compass) and whether someone has picked it up or knocked it over
(motion sensor). It sends a short message over a long-range radio network (Meshtastic), and
every other box and every phone on that network can see the alert.

## The parts, in plain terms

| Part | Think of it as | What it tells us |
|---|---|---|
| Radar x3 | Three motion-sensing "eyes" pointing different ways | "Something is 2.4 m away in sector B" |
| GPS | A satellite-fed clock and map | Where the box is, and the exact time |
| Accelerometer / gyro | The box's sense of balance | Level? Tilted? Being shaken? |
| Compass | A tiny magnetic needle | Which way the box faces |
| Pressure / temperature sensor | A weather station | Weather changes, rough altitude |
| RP2040 microcontroller | The brain | Reads everything, decides what matters |
| Meshtastic radio | The voice | Sends messages for miles with almost no power |

## What the brain does

1. **Listens** to all sensors many times a second.
2. **Filters** the radar: ignores very weak or very distant echoes so a leaf or a far-off car
   is not an alarm. You can tune how picky it is.
3. **Watches for tampering**: remembers how the box sat when it was armed, and raises an alarm
   if it tilts or is shaken.
4. **Turns changes into events**: "person appeared in sector B at 2.4 m", "box tilted 40 degrees".
5. **Reports** events (to USB today, to the mesh radio next) and heartbeat status on a schedule.

## Why three radars?

One radar sees a wedge, roughly 120 degrees wide. Three, pointed 120 degrees apart, cover the full
circle. The box also knows *which* radar fired, so the alert says roughly where, not just "something".

## Why a mesh?

A campsite perimeter is wider than a single radio can cover reliably, and there is no cell service.
Meshtastic is an open, low-power LoRa mesh: each box relays for its neighbours, so nodes can be
spread out, and a phone near any one of them sees everything.

## Settings live in the device

Every number that matters (how sensitive, how far, how long before alarming) is a **parameter**
stored inside the box's own memory. You change them over USB with a text command line or a
web page, and they survive power loss. Nothing needs recompiling.

## Standalone first

You can plug a box into a computer and use it fully with no mesh radio at all. That is deliberate:
the sensors and settings are proven on the bench first, and only then connected to the mesh.
