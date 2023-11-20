# USB-power-meter
Simple power usage monitor with just a split-coil current transformer and a ATtiny45 microcontroller.

## Hardware

To use this USB power usage monitor simply clamp a split-coil current transformer like the [SCT013-000](https://docs.openenergymonitor.org/electricity-monitoring/ct-sensors/yhdc-sct-013-000-ct-sensor-report.html) in the picture onto the electric cable to be measured.

<picture>
 <img alt="SCT013-000 and ATtiny45" src="https://github.com/rodajc/USB-power-meter/blob/main/images/ATTINY45%2BSCT013-000-100-small.jpg">
</picture>

The current transformer generates a current in the secondary coil that is proportional to the current going through the primary coil which is proportional to the [apparent power](https://en.wikipedia.org/wiki/AC_power#Apparent_power) usage assuming a resistive load.

The challenge is to measure the current and make it available through USB. The first step is to convert the current to a voltage by connecting a load resistor to the secondary coil. The result is an AC voltage.

The trick is to take many samples and calculate the average of the absolute values.

## ATtiny45 microcontroller

The bipolar differential ADC of the [ATtiny45 microcontroller](https://www.microchip.com/en-us/product/ATtiny45) can handle this without additional hardware. And connecting it to USB is very simple thanks to the [V-USB project](https://www.obdev.at/products/vusb/index.html). Only three resistors and a decoupling capacitor are needed.

<picture>
 <img alt="ATtiny45 circuit" src="https://github.com/rodajc/USB-power-meter/blob/main/images/tinysct-micro-small.png">
</picture>

## Firmware

The rest is done in firmware. Not even an external crystal or resonator is needed as the internal RC oscillator is used. It is calibrated by the firmware at power up.

The firmware samples during approx. 200 ms, i.e. 10 waves @ 50 Hz. The sum of the samples is divided by the number of samples (approx. 1600) to achieve the average voltage. This is [proportional](https://www.electronics-tutorials.ws/accircuits/average-voltage.html) to the RMS voltage which is proportional to the current and therefore to the power usage.

## Command line tool

On the host a command line tool called tinysct retrieves the result from the microcontroller through USB.

## Documentation

All documentation (e.g. how to calculate the load resistor), firmware, command line tool and schematics are available in this repository.

<picture>
 <img alt="SCT013-000 mounted in dsitribution panel" src="https://github.com/rodajc/USB-power-meter/blob/main/images/SCT013-000-mounted-small.jpg">
</picture>


