# USB-power-meter
Simple power usage monitor with just a split-coil current transformer and a ATtiny45 microcontroller

To use this USB power usage monitor simply clamp a split-coil current transformer like the SCT013-000 in the picture onto the electric cable to be measured.

<picture>
 <img alt="SCT013-000 and ATtiny45" src="https://github.com/rodajc/USB-power-meter/blob/main/images/ATTINY45%2BSCT013-000-100-small.jpg">
</picture>

The current transformer generates a current in the secondary coil that is proportional to the current going through the primary coil.

The challenge is to measure the current and make it available through USB. The first step it to convert the current to a voltage by connecting a load resistor to the secondary coil. The result is an AC voltage.

The trick is to take many samples and calculate the mean value of the absolute values.

The bipolar differential ADC of the [ATtiny45 microcontroller](https://www.microchip.com/en-us/product/ATtiny45) can handle this without additional hardware. And connecting it to USB is very simple too thanks to the [V-USB project](https://www.obdev.at/products/vusb/index.html). Only three resistors and a decoupling capacitor are needed.

The rest is done in firmware. Not even an external crystal or resonator is needed as the internal RC oscillator is used. It is calibrated by the firmware at power up.The firmware takes samples during approx. 200 ms, i.e. 10 waves @ 50 Hz. The sum of the samples is divided by the number of samples (approx. 1600) to achieve the average voltage. The average voltage is [proportional](https://www.electronics-tutorials.ws/accircuits/average-voltage.html) to the RMS voltage which is proportional to the current and therefore to the power usage.

On the host a command line tool called tinysct retrieves the result from the microcontroller through USB.

All documentation (e.g. how to calculate the load resistor), firmware, command line tool and schematics are available here on github.

