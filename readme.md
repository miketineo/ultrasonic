[##](##) Water Tank Level Automation

### Project

Arduino project to automate the water tank management based on the waterlevel reading using a couple of arduino boards, ulrtasonic sensors and some relays.

### Structure

The system is comprised of 2 parts, a Sender unit and the receiver unit. 

The sender is located next to the tank and reads from an ultrasonic sensor, the water level and updates the system by producing readings and publishing them via [MQTT](MQTT).

The receiver is another arduino board connected to the wifi network, subscribe to the water level update MQTT topic and reacts to it by turning on or off the main water pump that fills the tank.  The receiver also publishes to a separated topic to keep the pump states updated.

### Usage

Compile both parts separately, if using arduino cli

#### [Arduino](Arduino) CLI
```
cd SonicSender
arduino-cli compile arduino-cli compile --fqbn <board:fqbn> ./SonicSender.ino
arduino-cli upload -p <serial-comm-port> --fqbn <board:fqbn> ./SonicSender.ino
```
