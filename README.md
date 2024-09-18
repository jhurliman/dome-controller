# dome-controller

> _Software repository for the immersive dome control computer_

## Speaker Layout

The geodesic half-dome has 12 speakers, each with a unique name. The names are:

```
Alpha-One
Bravo-Two
Charlie-Three
Delta-Four
Echo-Five
Foxtrot-Six
Golf-Seven
Hotel-Eight
India-Nine
Juliet-Ten
Kilo-Eleven
Lima-Twelve
```

The layout is six speakers $X cm above the floor in a hexagonal pattern pointing towards the center of the dome (level elevation), four speakers $Y cm above the floor in a square pattern pointing towards the center of the dome (same elevation as the first hexagon level), and two speakers $Z cm above the floor in a short line pointing down towards the center of the dome (first elevation).

## Software / Hardware

MacOS Ableton Live + Envelop4Live -> Max4Live AmbiX decode -> MOTU 16-A -> 12-channel amplifier -> 12 6.25" speakers
..................................-> Chromatik (MIDI) -> ESP32 (Art-Net DMX) -> 10-addressable WS2805 ICs -> 30 RGB+CCT LEDs

## Getting Started

This repository is in the process of being overhauled, as the C++ code is being replaced with ESP32 firmware and Ableton/Max4Live/Chromatik project files.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
