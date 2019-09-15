# TTGO-T-Beam-WiFi-Connection

Uses the TTGO T-Beam LoRa board - v0.7

Monitors WiFi connection status and attempts a reconnect if WiFi connection is lost.
Visual inidcation (onboard LED) of WiFi connection lost.
Coded for SSD1306 which displays the time, data, WiFi status and time connected, WiFi lost counter and time lost.
vBat visual display on the SSD1306 - will notify of low battery (onboard LED) and will close WiFi if the battery is too low with a visual
on screen message. Once voltage is above a certain value WiFi is enabled again and a connection attempt made.

This is ongoing.

Current issues - 
Reconnect time has not been resolved yet, time displayed is there as a place holder at the moment.
