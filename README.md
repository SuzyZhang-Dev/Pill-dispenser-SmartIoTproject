# Pill Dispenser IoT Project

## Project Structure
```text
Pill_Dispenser_Project/
├── CMakeLists.txt              # CMake build configuration
├── README.md                   # Project documentation
├── lorareceive.py              # Python script for LoRaWAN data reception
├── .gitignore                  # Git ignore rules
├── docs/                       # Documentation files
│   ├── flowchart.md            # Detailed operation flowchart
│   ├── lora_commands.md        # LoRa AT commands reference
│   └── ...
├── src/                        # Source code
│   ├── main.c                  # Entry point (System Init & Main Loop)
│   ├── config.h                # GPIO mappings and global configuration
│   ├── drivers/                # Hardware Abstraction Layer (HAL)
│   │   ├── appkey.h            # LoRa AppKey (Not tracked by git)
│   │   ├── eeprom.c/h          # I2C EEPROM driver (Logs & State saving)
│   │   ├── encoder&button.c/h  # Rotary encoder & Button inputs
│   │   ├── iuart.c/h           # Interrupt-driven UART driver
│   │   ├── led.c/h             # PWM LED control (Breathing/Blinking)
│   │   ├── lora.c/h            # LoRaWAN logic (AT command wrapper)
│   │   ├── motor.c/h           # Stepper motor driver
│   │   ├── oled.c/h            # I2C OLED display driver
│   │   └── sensor.c/h          # Opto-fork & Piezo sensor driver
│   └── logic/                  # Business Logic Layer
│       ├── dispenser.c/h       # Dispenser mechanics (Calibration/Stepping)
│       └── statemachine.c/h    # Main State Machine (UI & Process Control)
```
Project Workflow:
```mermaid
flowchart TD
    %% Node Definitions
    Start((Power On))
    Init[System Init<br/>Load EEPROM State]
    CheckPowerOff{Power-off<br/>Recovery?}
    
    %% Welcome State
    StateWelcome[<b>STATE_WELCOME</b><br/>Select Mode via Encoder]
    StateLora[<b>STATE_LORA_CONNECT</b><br/>Joining LoRaWAN...]
    
    %% Main Menu
    StateMenu[<b>STATE_MAIN_MENU</b><br/>1. Get Pills<br/>2. Set Dose]
    StateSetPeriod[<b>STATE_SET_PERIOD</b><br/>Adjust Dosage Period 1-7]
    
    %% Calibration Flow
    StateWaitCalib[<b>STATE_WAIT_CALIBRATE</b><br/>Ready to Start/Resume]
    StateCalib[<b>STATE_CALIBRATE</b><br/>Running Calibration<br/>or Recovery Routine]
    
    %% Dispensing Flow
    StateDispense[<b>STATE_DISPENSING</b><br/>Dispensing Loop]
    CheckSuccess{Pill Dropped?}
    RetryCheck{Max Retries<br/>Reached?}
    LoopCheck{All Pills<br/>Dispensed?}
    
    %% Fault State
    StateFault[<b>STATE_FAULT_CHECK</b><br/>Error: DISPENSER EMPTY<br/>Send LoRa Alarm]
    ResetAction[User Press Encoder<br/>to Reset]

    %% Logic Connections
    Start --> Init --> CheckPowerOff
    
    %% Power-off Recovery Logic
    CheckPowerOff -- Yes --> StateWaitCalib
    CheckPowerOff -- No --> StateWelcome
    
    %% Mode Selection
    StateWelcome -- Mode: With LoRa --> StateLora
    StateWelcome -- Mode: Offline --> StateMenu
    
    StateLora -- Success/Fail/Timeout --> StateMenu
    
    %% Menu Navigation
    StateMenu -- Select: Set Dose --> StateSetPeriod
    StateSetPeriod -- Confirm --> StateMenu
    
    StateMenu -- Select: Get Pills --> StateWaitCalib
    StateWaitCalib -- Encoder Press --> StateCalib
    
    StateCalib -- Calibration Complete --> StateDispense
    
    %% Dispensing Logic
    StateDispense --> CheckSuccess
    
    CheckSuccess -- Yes --> LoopCheck
    CheckSuccess -- No --> RetryCheck
    
    %% Retry Mechanism
    RetryCheck -- No (Retry) --> StateDispense
    RetryCheck -- Yes (Fault) --> StateFault
    
    %% Loop Continuation
    LoopCheck -- No (Next Pill) --> StateDispense
    LoopCheck -- Yes (Job Finished) --> StateMenu
    
    %% Fault Recovery
    StateFault -- User Confirms --> ResetAction
    ResetAction --> StateWaitCalib

    %% Styling
    classDef state fill:#f9f,stroke:#333,stroke-width:2px;
    classDef decision fill:#ff9,stroke:#333,stroke-width:2px;
    
    class StateWelcome,StateLora,StateMenu,StateSetPeriod,StateWaitCalib,StateCalib,StateDispense,StateFault state;
    class CheckPowerOff,CheckSuccess,RetryCheck,LoopCheck decision;
```
