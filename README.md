# ESP8266_Alarm
Alarm panel monitor / signal notification via ESP8266 WiFi Module and ifttt.com.

Alarm Notify - Mike Russell May 2017. 
Hardware - ESP-01 with glue logic to existing Radionics alarm panel keypad (see schematic). 
Power Requirements - LM1117 3.3v ~15mA polling door, 70mA AP cfg mode.

Function: Takes a level converted door open signal from Radionics alarm keypad panel
LEDs and generates a HTTP trigger message to www.ifttt.com that will perform some
action based on that trigger message ie. send email or SMS text message.  Also
provide a local web page / access point to configure WiFi router and action
website info.  Code will provide local AP configuration page for 2 days if periodic
connect to network Access Point fails (reconfigure credentials for
connectivity).  If no AP config nor AP connect then go idle and wait for
next alarm control panel reset (unplug / plug in).

For general ESP-01 setup and Software Dev environment setup
reference this page:
www.randomnerdtutorials.com/door-status-monitor-using-the-esp8266/

My specific hardware implementation is shown in the project schematic.
I did not use a breadboard but rather soldered components directly
to Alarm Keypad Panel components and did point to point wiring/
soldering. Fortunately there was enough room in the Panel for the
ESP-01 and extra components.  The board with the LM1117 3.3v
regulator was epoxyed to the alarm panel board to attempt to
dissipate heat. With 10V drop across the LM1117 board it can
get quite hot in AP cfg mode (~70mA).  Not very warm in normal
operation (~15mA).

The code shuts down the ESP-01 RF when not needed to save power. The
ESP-01 can be jumper wire modified to support even lower power (microamps)
via deep sleep but since this is powered from the alarm panel and soldering fine
pitch surface mount is tough I did not make this mod.

This has been running well for a few months now.  You could also auto enable / 
disable the alarm (IFTTT applet) via proximity, time, or manually using Tasker on a
rooted Andoid phone. My phone detects distance from home and if greater than a
threshold then the IFTTT applet is enabled to send email if a door open event
is received. The on / off switch is accomplished via applet enable / disable while
the ESP-01 continues to send all door open events to IFTTT.com.  If I get a chance
I'll put that Tasker xml code also here on github.


