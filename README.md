---
title: Sliding Puzzle Next Move Suggesting Naive DL Model with ESP32 TensorFlow Lite
description: Take the DumbDisplay example game Sliding Puzzle adding to it AI option for next move
cover_image: /images/sliding-poster.png
tags: 'slidingpuzzle, esp32, tensorflowlite'
published: false
id: 2143402
---


# Sliding Puzzle 'Next Move' Suggesting Naive DL Model with ESP32 TensorFlow Lite

This [project](https://github.com/trevorwslee/ESP32SlidingPuzzle) takes the game[Sliding Puzzle](https://github.com/trevorwslee/Arduino-DumbDisplay/blob/master/examples/sliding_puzzle_w_suggest/sliding_puzzle_w_suggest.ino)
(with simple 'next move' suggesting 'search-directed heuristic' option),
adding to it the capability of suggesting 'next move' with a ***naive*** DL model realized with ESP32 TensorFow Lite support.
The Sliding Puzzle game is implemented for Arduino framework compatible microcontroller with the help of [DumbDisplay](https://github.com/trevorwslee/Arduino-DumbDisplay)
to render the game remotely on your Android mobile phone.
Specifically, ESP32 / ESP32-S3 is the targe microcontroller for this experiment, since it not only supports Arduino framework, it also supports TensorFlow Lite.


|  |  |  |
|--|--|--|
|![](images/sliding_ss_01.jpg)|🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>🤖<br>|![](images/sliding.gif)|

The DL model of this experiment is implemented with TensorFlow that I worked out by referencing to two of my previous experiments:
* [Trying Out TensorFlow Lite Hello World Model With ESP32 and DumbDisplay](https://www.instructables.com/Trying-Out-TensorFlow-Lite-Hello-World-Model-With-/)
* [Mnist Dataset -- From Training to Running With ESP32 / ESP32S3](https://www.instructables.com/Mnist-Dataset-From-Training-to-Running-With-ESP32-/)

In additional to an ESP32 / ESP32S3 microcontroller board, a few tools are assumed:
* Python 3
* Git
* VSCode and PlatformIO -- reference on how they are used: [A Way to Run Arduino Sketch With VSCode PlatformIO Directly](https://www.instructables.com/A-Way-to-Run-Arduino-Sketch-With-VSCode-PlatformIO/) 
* DumbDisplay Android App -- reference: [Blink Test With Virtual Display, DumbDisplay](https://www.instructables.com/Blink-Test-With-Virtual-Display-DumbDisplay/)


# Building DL Model with VSCode

First, clone this project's source from the project's [GitHub repository](https://github.com/trevorwslee/ESP32SlidingPuzzle)

```
git clone https://github.com/trevorwslee/ESP32SlidingPuzzle
```

Open the cloned directory `ESP32SlidingPuzzle` with VSCode, then open `train_model.ipynb`

![](.README_images/6fa19b9f.png)

Run all cells of the Jupyter Notebook `train_model.ipynb`.
If environment is not setup, it should first prompt you to create / select an Python environment. 


![](.README_images/9b64af69.png)

Create a virtual Python environment for the project

![](.README_images/3f4990e9.png)

![](.README_images/1ce7a270.png)

![](.README_images/7433b84a.png)

As the last step, when asked to install dependencies, ***make sure*** to select `requirement.txt`

![](.README_images/f8659f51.png)


This should create the Python virtual environment `.venv` for the project with the needed dependencies installed.

And if all the cells completed successfully, the model C header file `src/esp32_sliding_puzzle/sp_model_4.h` will be generated, overwriting the one already there.

Yes, there is one already included with this project, and hence it is not necessary for you to train the model again unless, you would like to tune the DL model (to be described the later section).

# Building and Uploading the Sketch

As mentioned previously, although not necessary, the steps to build and updated the sketch is via the PlatformIO extension.

The configurations for developing and building of the sketch are basically written down in the `platformio.ini` file
```
[env]
monitor_speed = 115200

[env:ESP32]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = huge_app.csv
monitor_filters = esp32_exception_decoder
lib_deps =
    https://github.com/trevorwslee/Arduino-DumbDisplay
    tanakamasayuki/TensorFlowLite_ESP32@^1.0.0
build_flags =
    -D FOR_ESP32

[env:ESP32S3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    https://github.com/trevorwslee/Arduino-DumbDisplay
    tanakamasayuki/TensorFlowLite_ESP32@^1.0.0
build_flags =
    -D FOR_ESP32S3
```
Notice that you have choices of PlatformIO environment -- ***ESP32*** and ***ESP32S3***

Please choose the correct one according to the microcontroller board that you have.

In either case, the program entry point is `src/main.cpp`
```
#include <Arduino.h>

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  #include "_secret.h"
#else
  #define BLUETOOTH "ESP32Sliding"
#endif

#include "esp32_sliding_puzzle/esp32_sliding_puzzle.ino"
```
Notice that
* For ESP32S3, WiFi connection to DumbDisplay Android app is used. In such a case, you will need to provide necessary WiFi login credential in a header file `src/_secret.h` that you need to create like content like
  ```
  #define WIFI_SSID           "wifi-ssid"
  #define WIFI_PASSWORD       "wifi-password"
  ```
* Otherwise, for ESP32, Bluetooth connection to DumbDisplay Android app is used, with the Bluetooth device name `ESP32Sliding`  

Build and upload the sketch with VSCode menu item ***View | Command Palette***

![](.README_images/2f6aa54f.png)

![](.README_images/163ce749.png)

For ESP32 which is using Bluetooth connectivity, you should see log entries with Serial Monitor (with baud rate 115200) like

![](.README_images/1490f900.png)

For ESP32S3 which is using Wifi connectivity, the log entries will be like

![](.README_images/ebaed7cb.png)

Notice that the IP to connect to the ESP32 board is listed

To connect your Android phone to your ESP32 / ESP32S3 microcontroller board via DumbDisplay Android app, open the DumbDisplay Android app and make connection like

| with Bluetooth  | add WiFi   | with Wifi |
|--|--|--|
|![](images/dd-connect-bt.jpg)|![](images/dd-connect-wifi-add.jpg)|![](images/dd-connect-wifi.jpg)|

# Sliding Puzzle Game UI

| | |
|--|--|
|After connection, you should see the picture of the board drawn.|![](images/sliding_ss_00.jpg)|
|To start a game, double click on the board, to divide the board into grid of tiles|![](images/sliding_ss_02.jpg)|

Double clicking on the board will randomize the board by 5 steps (5 random moves).

During game play, you can click ***Suggest*** for a suggested 'next move'.
If you can click ***Continuous*** suggested 'next moves' are continuously made until either you disabled ***Continuous*** or the game is completed. 

There are three option for the 'next move' suggestion:
1) ***AI Only*** -- use the trained DL model for predicting 'next move' (the highest probability one)
2) ***AI+Search*** -- largely use the trained DL model for predicting 'next move'; however, if the top 'next move' probability is not high, fallback to use original "search* algo 
3) ***Search Only*** -- use original "search" algo only

After every successful completion of the game, 5 more randomize steps are added to randomization making the game a bit harder.

# Improving the 'Next Move' Suggestion Accuracy

The 'next move' suggestion is certainly not very accurate, specially when the board is randomized for many steps.

* The ***naive*** DL model can certainly be improved
* The "search" algo can be improved
* The DL + "search" can be improved

***Wise that interested someones can try out and share how the 'next move' suggestion can be improved.***


# Enjoy!

Hope that you will have fun with it! Enjoy!

> Peace be with you!
> May God bless you!
> Jesus loves you!
> Amazing Grace!

