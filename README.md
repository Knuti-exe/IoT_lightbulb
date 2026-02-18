### Lightbulb control via MQTT

This is pretty simple project with seeed studio xiao ESP32-C3 and relay to control lightbulb. It can be used with any other component that you want to controll it power state (only if relays max current value is not exceeded). 

To power up relay and MCU, you will need some power source, like an old smartphone charger, which is connected in parallel with live wires to relay and lightbulb (one wire to lighbulb, the other one to the relay and any connector relay <-> lightbulb). DO NOT USE GROUND WIRE.

To check if everything is alright with ESP32, I used telnet to log some errors and informations.

# TODO
 - [x] README
 - [x] Light-sleep mode
 - [ ] Logging to files
 - [ ] Code refactor (optional)
