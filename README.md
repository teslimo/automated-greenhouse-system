# Automated Smart Greenhouse System for Year Round Farming

## Introduction

The Automated Smart Greenhouse System is designed to optimize plant
growth by monitoring and controlling essential environmental parameters
such as temperature, humidity, soil moisture, and light intensity. By
integrating an ESP32 microcontroller with multiple sensors and
actuators, the system ensures real-time automation and energy-efficient
operation, reducing manual labor while enhancing crop yield and
consistency.

------------------------------------------------------------------------

## Project Overview

This project focuses on building an automated greenhouse
system** that leverages embedded systems and automation to support
precision agriculture. It stores crop-specific optimal values in memory,
reads sensor data, and automatically actuates devices like pumps, fans,
humidifiers, and grow lights.

The system features a **16x4 LCD interface with keypad navigation** for
crop selection and parameter display. 

------------------------------------------------------------------------

## Key Features

-   🌱 **Crop-specific parameter storage** in EEPROM (temperature,
    humidity, soil moisture).
-   📟 **User Interface:** 16x4 LCD display with keypad navigation for
    crop selection.
-   🌡️ **Environmental Monitoring:** Real-time tracking of temperature,
    humidity, and soil moisture.
-   ⚡ **Automated Actuation:**
    -   Fan → temperature and humidity regulation
    -   Pump → soil moisture regulation
    -   Humidifier → temperature and humidity regulation
    -   Grow light → crop selection based

-   🔋 **Efficient Power Management:** Multiple buck converters to
    distribute power between 5V and 12V devices.
-   ⚙️ **Expandable Design:** Can integrate cloud platforms (ThingSpeak
    or Blynk) for remote monitoring.

------------------------------------------------------------------------

## How It Works

1.  **Startup:**
    -   On power-up, the system checks EEPROM for stored crop
        information.
    -   If found, the crop and its optimal environmental parameters are
        displayed.
    -   User can enter selection mode by pressing the **D key** and
        scroll through crops using **E**.
2.  **Crop Selection & Storage:**
    -   Crops are displayed on different lines of the LCD.
    -   Once a crop is chosen, pressing **A** confirms and saves the
        selection in EEPROM.
3.  **Real-Time Monitoring:**
    -   Sensor data is continuously displayed on the LCD.
    -   Wi-Fi-based RTC keeps accurate system time visible at the top of
        the display.
4.  **Automation:**
    -   Fan and humidifier are regulated for smooth
        operation.
    -   Pump and grow light are managed by active-low relays or
        transistor drivers.
    -   Grow light turns on/off based on crop selection(outdoor or indoor).
    -   Heat lamp intensity controlled with custom built ac dimmer circuit.

------------------------------------------------------------------------

## Components Used

  ----------------------------------------------------------------------------
  **Component**   **Description**                     **Function in System**
  --------------- ----------------------------------- ------------------------
  **ESP32**       Wi-Fi-enabled microcontroller with  Central controller for
                  GPIOs and ADCs                      sensors, actuators, and
                                                      logic

  **DHT11         Digital temperature and humidity    Measures temperature and
  Sensor**        sensor                              humidity

  **Soil Moisture Analog soil moisture detection      Monitor soil moisture
  Sensors (2x)**  probes                              levels for irrigation
                                                      control

  **16x4 LCD      Liquid crystal display with I2C     Displays crop info,
  Display (I2C)** backpack                            sensor values, and time

  **Keypad (4x4   Matrix keypad with I2C interface    User input for crop
  I2C)**                                              selection and navigation

  **IRLZ44N       Relay driver board for AC/DC        Switch devices(Grow light, 
    MOSFET                                              fan, humidifier, pump)
                                       
  **DC Pump       Mini water pump                     Irrigation based on soil
  (5V)**                                              moisture

  **Fan (12V)**   Cooling fan                         Temperature regulation

  **Ultrasonic    Mist generator                      Increases humidity
  Humidifier                                          
  (5V)**                                              

  **LED Grow      Artificial lighting                 Provides light for
  Light (12V)**                                       photosynthesis

  **Buck          DC-DC step-down converters          Power distribution to
  Converters (5V                                      ESP32, sensors, and
  & 12V)**                                            actuators

  **EEPROM        Non-volatile memory                 Stores crop selection
  (Internal to                                        and optimal parameters
  ESP32)**                                       

  **Heat lamp**   intensity control                   Regurates temperature and 
                  via dimmer circuit                  humidity of greenhouse
  ----------------------------------------------------------------------------
