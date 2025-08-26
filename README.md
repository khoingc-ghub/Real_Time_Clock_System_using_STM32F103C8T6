# Design-of-a-Real-Time-Clock-System-using-STM32F103C8T6-DS3231-and-ST7735-TFT-LCD-
# STM32 Real-Time Clock with Alarm & Timezone Display

## 📌 Overview
This project demonstrates how to build a **digital clock system** using an STM32 microcontroller, a **DS3231 RTC module**, and an **ST7735 TFT LCD display**.  
The system can:
- Display the current time, date, and temperature.
- Set and trigger an alarm with buzzer notification.
- Adjust and save the system time.
- Display multiple time zones (Vietnam, Japan, UK, USA).
## ⚡ Features
- **RTC Synchronization**: Reads time from DS3231 via I2C.
- **Alarm Functionality**: User-configurable alarm with buzzer sound.
- **User Interface**: Push buttons for setting time/alarm and switching modes.
- **Timezone Conversion**: Display time in 4 different zones.
- **LCD UI**: Visual interface on 1.8" ST7735 TFT display.

## 🎛️ Modes
The system supports 4 operation modes:
1. **Normal Mode** – Show current time, date, temperature.
2. **Set Alarm Mode** – Configure alarm (hour & minute).
3. **Set Time Mode** – Adjust system time and date (hour, minute, second, day, month, year).
4. **Timezone Mode** – Display world clock for different time zones.

## 🖥️ Hardware Requirements
- STM32F103C8 (Blue Pill) or similar STM32 board
- DS3231 Real-Time Clock module (I2C)
- ST7735 TFT LCD (SPI)
- Buzzer (GPIO)
- 4 push buttons for navigation and settings

## 🔧 Pin Configuration
| Peripheral | Pin | Description |
|------------|-----|-------------|
| DS3231 (I2C1) | PB6 (SCL), PB7 (SDA) | I2C communication |
| ST7735 (SPI1) | PA5 (SCK), PA7 (MOSI), PB1 (CS), PB10 (DC), PB0 (RESET) | SPI communication |
| Buttons | PA1, PA2, PA3, PA6 | Mode, Confirm, Increase, Move |
| Buzzer | PA8 | Alarm sound |
| SQW/INT (RTC) | PA4 | Alarm interrupt input |

## 📷 Screens
- **Normal Mode**: Displays `HH:MM:SS`, date, temperature.
- **Alarm Mode**: Blinking time for adjustment.
- **Set Time Mode**: Editable fields with blinking effect.
- **Timezone Mode**: Shows time with country flag.
