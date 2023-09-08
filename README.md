# psoc_pong

Two player pong game using 2x CY8CKIT-062S2 and 2x CY8CKIT-028-TFT.

Each kit has a TFT display with a paddle that is controlled by the CAPSENSE slider.

Bluetooth LE is used to send the ball back and forth between kits. One kit is
programmed as tle Bluetooth Central and the other is programmed as the Bluetooth
Peripheral. The central automatically connects to the peripheral by name at power-up.

The angle and speed of the ball after it hits a paddle is randomly changed.

A new game begins automatically as soon as one of the paddles fails to send the ball back.
