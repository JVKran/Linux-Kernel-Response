# HPS Linux Software
This repository contains the kernel modules required to operate the custom Platform Designer components as implemented in the Hard Processor System of my [Configurable Response Meter](https://github.com/JVKran/Configurable-Reaction-Meter/tree/main/hps). Apart from these modules, I've also provided an example application that, as the name suggests, implements a response time meter.

## Getting started
Make sure the DE1-SoC has been configured and loaded with the [Platform Designer system](https://github.com/JVKran/Configurable-Reaction-Meter/tree/main/hps) that's running the Arch-Linux image provided by Intel or the Hogeschool Rotterdam. With Unbuntu booted, one should clone this repository. Since the executables are also uploaded to git, one doesn't have to compile anything; just execute ```sudo ./start.sh``` to install all kernel modules. This command shouldn't return any errors.

With all kernel modules installed correctly, one is almost able to run ```./app/main```. First, check out the macros in the [main.c](app/main.c) and update them according to your ThingsBoard installation. Now, go ahead and run the application!