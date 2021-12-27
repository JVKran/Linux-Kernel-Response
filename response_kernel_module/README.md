# Response module
This kernel module can be used in correspondance with my custom [Configurable Response Meter](https://github.com/JVKran/Configurable-Reaction-Meter/tree/main/components/reaction_meter). 

## Usage
Use ```head -c4 /dev/response*``` to read the last response time. Please note that the first character may be repeated when the user responds in less than 1000ms due to catting 3 characters.

Use ```echo "2500" > /dev/response*``` to specifiy an initial delay of 2500 milliseconds. This makes the meter wait 2500 milliseconds after KEY_0 is pressed. When the leds start lighting up, press KEY_1 as fast as possible. This is only doable with an application that controls the leds and seven segment displays.

The hardware generates interrupts whenever a state transition takes place. The state identifier is passed as signal information to the user-space irq handler. Depending on the state, one can propagate through the application level state-machine or read and display the response time.
