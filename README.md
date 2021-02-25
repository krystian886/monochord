# monochord

List of content

- [Intro](#Intro)
- [Description](#Description)
- [Detailed_info](#Detailed_info)
    - [monochord](#monochord)
    - [recorder](#recorder)
    - [steering_recorder](#steering_recorder)
    - [info_recorder](#info_recorder)
- [Screenshots](#Screenshots)
    
# Intro

Project created for the university course - Linux programming

Created on Ubuntu 20.04 LTS, tested on Slackware 14.2

To compile the programs I used:

gcc -Wall <prog_name>

**!!Attention!!** monochord.c also requires math library:

gcc -Wall monochord.c -lm

This programme was created using the basis of:

linux signals, clocks, sockets, file managing and data parsing.



Side note: Programs are created without header files (.h) on purpose.


# Description

This programme consists of four subprograms and uses the help of the netcat (nc).

Monochord (controlled by netcat - UDP socket) simulates sine values which can be send further
with the help of Real-Time Linux signals.

These signals can be received by the recorder and written on stdout or specified file
as text and/or binary data.

Recorder is controlled by steering_recorder and its working status can be checked using
info_recorder.


# Detailed_info

## monochord

params: <short int> UDP port number

example: ./monochord 8888

It contains six values defining options of sine creation and options of sending them via Real-Time signal.

float amp - sine amplitude								default: 1.0
float freq - sine frequency 							default: 0.25
float probe - sine probe frequency in sec				default: 1
float period - how long does the probe lasts in sec		default: -1			special values: 0 - infinity, < 0 - stop
short pid - recorder pid								default: 0
short rt - Real-Time signal which to use to send data	default: 0


These values can be modified via **nc**. Examples:

nc -u 127.0.0.1 <port number the same as in monochord params> "amp 10"

nc values can be sent in one of four ways: "key value", "key: value", "key(tab)value", "key:(tab)value".


When needed, status of these values can be checked by sending "report" on netcast.


## recorder

params: 

	-b  (optional) binary file path (if given '-' it will use stdout)
	
	-t  (optional) text file path (if not providen, it will use stdout)
	
	-d  <int> number of sine data sending signal
	
	-c  <int> number of steering signal
	
example: ./recorder -b - -d40 -c41

Serves sine data comming from monochord. It's work parameters can be modified by the steering_recorder programme.


## steering_recorder

params:

	-p  recorder pid
	
	-s  <int> number of steering signal
	
	-v  recorder steering value 
	
example: ./steering_recorder -p 8903 -s 41 -v 1

Recorder can be controlled using special steering values:
	stop: 0 - resets everything
	
	start: 1,
	
	+ 0 - using world clock
	
	+ 1 - new reference time point
	
	+ 2 - previous reference time point or new if previous does not exists
	
	+ 4 - add monochord pid to the saved data
	
	+ 8 - clear files and start once again (works only when writing to the files)


## info_recorder

params:

	-c <int> number of steering signal
	
	recorder pid
	
example: ./info_recorder -c 41 8903

Sends steering signal with value 255 what triggers recorder to send back its current state info.



# Screenshots

![sceenshot_1](/readme_files/screenshot_1.png)

![sceenshot_2](/readme_files/screenshot_2.png)

![sceenshot_3](/readme_files/screenshot_3.png)

![sceenshot_3](/readme_files/screenshot_4.png)
