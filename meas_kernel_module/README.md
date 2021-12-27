# Measurement module
This kernel module can be used in correspondance with a standard [Parallel IO Peripheral](https://github.com/JVKran/Configurable-Reaction-Meter/tree/main/components/seven_segment_controller) from Alterra.

## Usage
Use ```echo "0x02" > /dev/leds*``` to turn on second led. Furthermore, a logic analyzer can be hooked up to it to visualize the states of the state machine.
