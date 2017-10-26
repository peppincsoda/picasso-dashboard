
# What is this?

This application can display an RPM gauge in your car.
(My car did not have an RPM gauge by design.)

The software is platform independent and open-source.
It uses Qt5: Technically the dial on the screen is a CircularGauge instance from QtQuick 2 Extras
and all serial communication is handled asynchronously by QSerialPort.

## Demo

You can watch it in action here:

[![IMAGE ALT TEXT HERE](http://img.youtube.com/vi/ELJv8D_VtW4/0.jpg)](http://www.youtube.com/watch?v=ELJv8D_VtW4)

The hardware setup for this demo was the following:

+ Raspberry Pi 3 Model B
+ Adafruit HDMI 5" 800x480 Display Backpack (https://www.adafruit.com/product/2260)
+ ELM327 OBDII interface (Chinese clone)

## License

MIT

Enjoy!
