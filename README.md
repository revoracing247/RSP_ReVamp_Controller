<p align="center">
	<img src="https://capsule-render.vercel.app/api?type=waving&&animation=fadeIncolor=gradient&height=300&section=header&text=Re-Vamp%20Controller!&fontSize=80" />
</p>

## Notes
- Not sure if this project will make it into a YT video or not. we'll see.
- Arduino IDE needs ESP32S3 boards installed.
- I'm pretty sure I'm not getting anywhere close to what these processors can do.

## Technical Notes
- Steering/Throttle have auto adjusting outer limits so they will stick to new min/max.
- Steering/Throttle Trims are behind a menu and saved to persistant memory so you can use those as separate axis in games.
- Vibrator output on the board can be piped to any other load (inc. inductive loads) that is 3.3V and up to 250mA.
- ADC voltages are good estimates, but could be fine tuned for each board if desired.
- Battery percentage reported to BLE allows system to operate with voltage dips on ESP32 transmits. This might be hard on the hardware.

## Arduino IDE libraries needed (can be installed through IDE)
- Wired built on https://github.com/schnoog/Joystick_ESP32S2
- Wireless built on https://github.com/lemmingDev/ESP32-BLE-Gamepad
- ESP32-BLE-Gamepad might also need https://github.com/h2zero/NimBLE-Arduino

## License (Draft!)
Published under the MIT license. Please see license.txt.

It would be great however if any improvements are fed back into this version.