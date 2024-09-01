# ESP8266 Moisture measurement

## Setup development Environment

Follow instructions [here](https://docs.espressif.com/projects/esp8266-rtos-sdk/en/latest/get-started/linux-setup.html)

`mkdir -p esp8266;`

`cd esp8266;`

`source .venv/bin/activate;`

`export IDF_PATH=$HOME/esp8266/ESP8266_RTOS_SDK`

`PATH="$PATH:$HOME/esp8266/xtensa-lx106-elf/bin"`

## Compile and upload Code

Connect your esp8266

`make menuconfig`

Go to "Sensor Connection Configuration" and add your wifi credentials and push gateway host address

To build, upload and monitor the project run

`make flash monitor -j8 -i`

To build only run

`make app`
