# SpaceVision3000



**SpaceVision 3000** is a compact desktop visualizer for various space-related data and imagery using ESP32, APIs, and wifi. 

This project uses a PCB that was originally for a diferent project, namely https://github.com/supermakesomething/mini-video-player

I wired up a few of them but then decided to try out some of the ESP32's other functions, like WIfi connectivity, and using APIs to collect data from the internet.

If you like this project please consider leaving me a subscribe at my [YouTube page](https://youtube.com/@CascoBayStudios). I mostly make toy elevators so you can check out a bunch of videos on those, over there! So, on to the SV3K! 

![PXL_20260213_232950658 1](https://github.com/user-attachments/assets/b21051a5-1ddb-4fcf-afba-62574dca9b55)

## What does it do?

**SV3K** displays data and imagery from various sources. You can click thru the data screens with the NEXT button, and display metadata with the PREV button. In order the screens show:

- Astronomy PIcture of the Day (NASA)
- The next 3 rocket launches worldwide (thespacedevs.com)
- The most recent exoplanets discovered (NASA)
- The earth from space, single view and multi-view (NASA)
- Different views of the sun (NASA)

It also has 2 different screensavers after 10 minutes of inactivity, a blackhole moving thru the void, and a starfall that dissolves the current screen. 

The code will check for updates to the images, launches, and exoplanet data

## Assembly and coding

The project requires the same materials as used in the Mini Video Player at the link above. I made it a bit differently, by soldering female headers on for the ST7735 and female stacking headers for the ESP32 on the back of the board. This was so if I fried the ST7735 by accident (voice of experience) or the ESP32 I could replace it with a new one. I also designed a small enclosure for it and 3D printed it but it is fairly specific to the way I assembled the board. I think future versions could use the PCB but wire the buttons and pots and power switch to the outside of the enclosure for a more polished look. 

I also added a [Sparkfun LiPo charger/booster](https://www.sparkfun.com/sparkfun-lipo-charger-booster-5v-1a.html) and a LiPo battery to the setup. 

The code was created with the help of various AI tools, with a lot of tweaking. The final code is probably prettty clunky but it works! 

You will need to include several libraries, and the custom font, LuckiestGuy (or other font you want to use). Include the font as a separate tab with your code, in the Arduino IDE. Libraries needed: 
--WiFi
--HTTPClient
--WiFiClientSecure
--ArduinoJson
--TFT_eSPI
--TJpg_Decoder
--time
--WiFiManager

WIth TFT_eSPI you also need to configure the User_Setup.h file to match your display. If you use the same screen as in the Mini VIdeo PLayer you just set it up as a 160 x 128 st7735, green tab version. I used GREENTAB2 but I think others will work too. 

## Wifi connectivity

**API:** You will need to get a NASA API in order to retrieve some of the data. It is free and very easy to do, just go to [https://api.nasa.gov/](https://api.nasa.gov/). Once you have the API, plug it into the code, here: `const char* nasaKey = "YOUR NASA API";.` 

**WIFI:** At first boot, the ESP32 will create a wifi hotspot (access point), that you will need to connect to in order to connect the ESP32 to your wifi network. You can change the name of the access point and password if you want. 

There are plenty of parameters to play with, including the colors of the text, the number of stars in the screensavers, speed of teh black hole, etc. You could also add other screens, sine NASA has a bunch more data points yuou can retrieve like Near Earth Objects and other satellite imagery. 
