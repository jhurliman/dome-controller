# dome-controller

> _Software repository for The Chiller_

## Speaker Layout

The geodesic half-dome has 12 speakers, each with a unique name. In a Y-up coordinate system, the speaker locations are as follows:

```
Speaker         x (m)      y (m)     z (m)
--------------------------------------------
Alpha-One       1.8288     0.0       0.0
Bravo-Two       0.9144     0.0       1.58
Charlie-Three  -0.9144     0.0       1.58
Delta-Four     -1.8288     0.0       0.0
Echo-Five      -0.9144     0.0      -1.58
Foxtrot-Six     0.9144     0.0      -1.58
Golf-Seven     -1.8288     1.83     -1.83
Hotel-Eight     1.8288     1.83     -1.83
India-Nine      1.8288     1.8288    1.8288
Juliet-Ten     -1.8288     1.83      1.83
Kilo-Eleven     0.0        2.13      0.5
Lima-Twelve     0.0        2.13     -0.5
```

And in AED (Azimuth, Elevation, Distance) format that Envelop4Live uses:

```
aed 1 0 0 1.8288,
aed 2 -60 0 1.8256,
aed 3 -120 0 1.8256,
aed 4 -180 0 1.8288,
aed 5 120 0 1.8256,
aed 6 60 0 1.8256,
aed 7 135 35 3.169,
aed 8 45 35 3.169,
aed 9 -45 35 3.169,
aed 10 -135 35 3.169,
aed 11 -90 76 2.187,
aed 12 90 76 2.187
```

The layout is six speakers 50 cm above the floor in a hexagonal pattern pointing towards the center of the dome (level elevation), four speakers 200 cm above the floor in a square pattern pointing towards the center of the dome, and two speakers 250 cm above the floor in a short line pointing down towards the center of the dome.

## Software / Hardware

MacOS Ableton Live + Envelop4Live -> Max4Live AmbiX decode -> MOTU 16-A -> 12-channel amplifier -> 12 6.25" speakers
..................................-> Chromatik (MIDI) -> ESP32 (Art-Net DMX) -> 10-addressable WS2805 ICs -> 30 RGB+CCT LEDs

## License

This project is licensed under the Affero General Public License v3.0 - see the [LICENSE](LICENSE) file for details.
