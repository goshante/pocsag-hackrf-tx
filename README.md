# POCSAG C++ Implementation & Implementation of HackRF transmitter
Full implementation of POCSAG protocol on C++ that produces encoded pager message modulated in PCM (Wave) or in raw data. And also feature to transmit it via HackRF. Very easy to use and to build. 
<br />You can stream your PCM data or WAV files via radio. Or make your own paging station and software.
<br />
<br />If you are looking for some GUI tool that can send messages to pagers from Windows, you can check my GUI Pager sender based on this project here:
<br />https://github.com/goshante/hackrf-pocsag-gui
## About POCSAG
POCSAG is the most commonly used protocol for pagers. In this repository you can find **POCSAG.cpp** and **POCSAG.h** sources. This is full implementation of POCSAG encoder. You don't need any 3rd party additional libraries to use it. This libary is ready to use juse after copying it into your project.
<br />
<br />Implemented according to this article:
<br />http://www.rfcandy.biz/communication/pocsag.html
<br />
<br />And this specification:
<br />https://www.raveon.com/pdfiles/AN142(POCSAG).pdf
<br />
<br />Tested on real pagers and it works fine. Supports text, numeric and tone messages. This library can produce raw output of bytes (bits) or modulated PCM audio buffer that is ready to be sent via FM transmitter.
<br />
<br />You are free to use this library in your projects, but only if you credit me and this repository in your project + your repository.

## About HackRF transmitter
This is fully working HackRF FM transmitter. The example how to use it you can see in **main.cpp** file. Very easy to use. You can send any PCM samples, sounds, music and even data in FM moduiation via your HackRF. FSK is supported, but only as a PCM samples. Works as a queue of chunks in internal thread, you can push new chunks while transmitting, so it's possible to make live streaming software for HackRF with this. To use this library you need **libhackrf**, **libusb** and **pthread**. It's very easy to build on Windows too! I'l help you with this below.
<br />
<br />Based on this project, but now my project has almost nothing common with it's origin. Deep refactoring was done and almost all code is rewritten. Removed most of all C-style code snippets, unused garbage, etc.
<br />Project link:
<br />https://github.com/gsj0791/HackRF_FM_Transmitter
<br />
## How to build
To build this project you need to build libhack rf:
https://github.com/greatscottgadgets/hackrf/tree/master/host/libhackrf/src

### You are lazy
If you are too lazy just use my build from **libhackrf.zip** from this repository. It has pre-built dll compiled with MSVC v143 compiler for **x64** and **x86_x32** platforms both Release and Debug versions. Just exctract it somewhere and specify path in your project settings.

### You want to build libhackrf
If you don't want to mess with CMake on Windows (that is always garbage on Windows) just create an empty project in Visual Studio, select .lib **(or even better .dll)** type of binary and subsystem Windows in linker settings if you building .dll. And just copy-paste hackrf.c and hackrf.h into your project. 
<br />
<br />At the first of all you need to build libusb. You can download sources here:
<br />https://github.com/libusb/libusb
<br />
<br />To build it just go to msvc folder and you find ready to use VS project files. It always builds fast with no problems. **You need static build for your platform of libusb**, other stuff is redundant.
<br />
<br />Then you need to build pthread, this is pretty easy too. Download it here:
<br />https://github.com/GerHobbelt/pthread-win32
<br />
<br />Everything like libusb, go to windows folder and open VS project. Make static build of this library for your platform.
<br />
<br />sNow you can build libhackrf. Just add include and lib pathes to libhackrf project, specify .lib files in your linker settings (if you building dll) and build your library.
Now you can specify path lo libhackrf.lib library and libhachrf.h include.
If you use .DLL just put it near your .exe and enjoy.