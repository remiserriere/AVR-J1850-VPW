# AVR-J1850-VPW
J1850-VPW code for AVR (ATmega8/16/32 in mind) to mimic (and overpass) the ELM322 now reaching its end of life.

## Original credits
Original credits go to Michael at www.mictronics.de. This project has been taken down by him on his server and archives, but I could not let this die. 

## What is this project?
The goal behind this project is to build an interface similar to the ELM322 from ElmElectronics (out of business in June 2022). 
The ELM322, based on a PIC12C509A microcontroler, is a simple J1850-VPW interface that works great with Chrysler PCI bus. It is (or was) a 8 pins chip, easy to integrate in a circuit, requiring minimum hardware:
* A NTSC 3.5795 Mhz crystal.
* Some transistors to handle the PCI bus voltage.
* Either a 8 volts zener diode or voltage regulator to power the PCI bus.

Michael's AVR J1850-VPW interface was based on an ATmega8 microcontroler. 
This little bugger is more than enough for the task! However it differs from the ELM322 by its pinout and crystal frequency (7.3728 Mhz). While the circuitry around the microcontroler is different in Michael's initial project, the schematic provided by ElmElectronics for the ELM322 works more than fine with this interface. 

## About the code...
The code here is basically what Michael wrote in the first place. Only few bits here and there have been updated to, first, match the ELM322 v2.0:
* Carriage return only is used by default (linefeed can be enabled by setting the ATL parameter to 1).
* New lines (\r and \l if required) have been reorganized to math the ELM322 output. 

On top of that, some features have been improved or added:
* A new "check for message length" ATC0/1 command allows you to verify if the message's length to be sent on the PCI bus is compliant with the SAE J1850 standard (should be less than 12 bytes long including CRC). This setting is disabled by default.
* A new "send direct" ATSD command allows you to send an entire message on the bus, where you can define the whole bytes of the message. The header will not be used. In other words you can send "ATSD24402f380201" to trigger the BCM Chime actuator, for example. However this command **does not support read nor wait for an answer form the target module**. It only sends a command. This can be usefull for flooding the bus, or triggering actuators without caring of the answer.
* The "set header" ATSH command accepts 1-byte headers and automatically configures the chip to run in 1-byte mode,
  * Michael's original code would also require the "1-byte header" ATO command to be set to 1 as well.
* This might only be matching my needs but... if ATSH XXyyzz is used to set a header and XX & 0x04 then the receiver address is set to the **second** byte of the header,
  * While this is not the default J1850-VPW protocol standard where the receiver address should be set to the 2nd header byte + 0x01, this is matching Chrysler diagnostic data where:
    * any successful command ran against header 24-40-22-aa-bb-cc...
      * sent by address 0x24 which is a typical DRB3 scan tool address
      * to module at address 0x40, this is the BCM
      * calling a function 0x22, in this case we are pooling for a sensor value
      * with parameters aa, bb, cc (the sensor ID)
    *  ...would receive an answer such as 26-40-62-dd-ee-ff-crc
       * answer from a module to the scan tool: 0x26 = scan tool address + 0x02 = 0x24 + 0x02
       * receiver address mathes the module we are communicating with, 0x40, the BCM
       * function returned a valid value (function address + 0x40 = 0x22 + 0x40 = 0x62) - in most cases a 0x2f would mean the function failed to execute, 0x3f would mean access to the function was denied
       * dd-ee-ff and crc are the returned values (sensor value here) and crc.

## Ok now how do I implement the hardware?
Well you can find the latest available ELM322 datasheet in this repository as a PDF file. The schematics provided by ElmElectronics have everything you need. You will also find Michael's schematic in the datasheets section. 

While Michael's schematic may be easier to implement than the **second** ELM322 schematic, this last one is a lot more robust and reliable. For example, if you are flashing the ATmega while connected to your car PCI bus, the bus can be driven low during this time. This is not a huge issue because the PCI bus itself is protected against shorts to ground (you should not harm any thing). However during this time the bus will be silent, modules will not be able to communicate. 
ElmElectronics schematic #2 is safer, the bus will not be driven low during ATmega flashes. 

Note that the OBDin and OBDout as well as TX and RX pins will differ from the ELM322 to the ATmega. If needed, have a look at the schematic folder to see how I implemented this chip. 