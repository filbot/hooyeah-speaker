# hooyeah-speaker

Hooyeah Speaker is a Wi‑Fi‑connected, vintage‑styled speaker that plays pre‑loaded sound effects in response to web events. The firmware runs on an ESP8266, which connects to the network and exposes a tiny HTTP endpoint. When a webhook is received, the ESP8266 briefly pulls a GPIO line low to ground a trigger input on an Adafruit Audio FX Sound Board, which then plays the corresponding sound stored in its onboard flash.

The system is designed to be simple, responsive, and robust for ambient notifications. It uses a short, bounded low pulse to emulate an open‑drain trigger, leaving the line high‑impedance at idle to avoid false activations. Optional request token support provides lightweight gating for local integrations, and mDNS announces the device on the LAN for easy discovery.

Typical uses include reacting to CI build results, chat events, or smart‑home signals with tactile audio cues. This repository contains the Arduino sketch for the webhook server/trigger logic, a secrets template for Wi‑Fi credentials, and contributor guidelines that define structure and style as the project grows.
