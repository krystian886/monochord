# monochord

List of content

- [Intro](#Intro)
- [Description](#Description)
- [Detailed_info](#Detailed_info)
    - [monochord](#monochord)
    - [recorder](#recorder)
    - [info_recorder](#info_recorder)
    - [steering_recorder](#steering_recorder)
    
# Intro

Project created for university course - Linux programming

Created on Ubuntu 20.04 LTS, tested on Slackware 14.2

To compile the programs I used:
	gcc -Wall <prog_name>

**Attention** monochord.c also requires math library:
	gcc -Wall monochord.c -lm

This programme was created using the basis of:
	linux signals, clocks, sockets, file managing and data parsing


# Description

This programme consists of four subprograms and uses the help of the netcat (nc).

**Quick intro**
Monochord (controlled by netcat - UDP socket) simulates sine values which can be send further
with the help of Real-Time Linux signals.

These signals can be received by the recorder and written on stdout or specified file
as text and/or binary data

Recorder is controlled by steering_recorder and its working status can be checked using
info_recorder


# Detailed_info

## monochord

## recorder

## info_recorder

## steering_recorder
