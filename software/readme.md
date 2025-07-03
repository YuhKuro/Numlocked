# Software

The windows software for numlocked comes with two parts. A frontend GUI and a backend that runs in the background of the computer. 
## Frontend

The GUI is the following:
![GUI Example](pictures/GUIExample.png)

The GUI allows the user to customize the OLED screen located on the keyboard, as well as activate game-mode, which disables the windows key and boosts scanning performance.

The user can save their settings, add new game mode applications for automatic game mode activation and set their timezone. 

In the future, being able to upload images to set the OLED image is planned.

The frontend is written in Python using PyQT. 

## Backend

The backend handles USB communication. It connects to the frontend via sockets, and passes data down to the keyboard's CDC. It will, if enabled, scan windows for running apps and automatically enable game mode. It will also update the keyboard on the current time for the timezone it is responsible for. 

The backend is written in C, with help from the cJSON library and heavily uses windows libraries. Thus, making a linux version is necessary at some point.