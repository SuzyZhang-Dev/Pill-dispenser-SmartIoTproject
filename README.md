```
Pill_Dispenser_Project/
├── CMakeLists.txt              
├── README.md                   # Introduction
├── docs/                       # no-code documents
│   ├── flowchart.md            # Operating principles
│   ├── reflection.md           # Reflection/Division of work
│   ├── lora_commands.md        # LoRa AT
│   ├── lorareceive.py          # Example code to verify LoRa connection
│   └── images/                 # screenshots and else
│     
├── src/                        
│   ├── main.c                  
│   ├── config.h                # GPIO
│   ├── drivers/                
│   │   ├── motor.c             # Stepper Motor
│   │   ├── motor.h
│   │   ├── sensors.c           # Opto Fork & Piezo
│   │   ├── sensors.h
│   │   ├── lora.c              # LoRaWAN via UART
│   │   ├── lora.h
│   │   ├── eeprom.c            # I2C EEPROM
│   │   ├── eeprom.h
│   │   ├── ui.c                # LEDs & Button
│   │   └── ui.h
│   └── logic/                  
│       ├── dispenser.c         # calibration, statemachine
│       └── dispenser.h
```

Project Workflow:
```mermaid
flowchart TD
A[Power on<br/>OLED shows welcome message<br/>LED blinking at 30% brightness<br/>Waiting for encoder press] --> B[User presses encoder<br/>Enter dosage setup]

    B --> C[Set dosage per intake<br/>Rotate encoder to adjust value]
    C --> D[Press encoder again to confirm dosage]

    D --> E[Screen shows “Ready for calibration”<br/>LED keeps blinking]
    E --> F[Press encoder to start calibration]
    F --> G[Run calibration routine]
    G --> H[Calibration finished<br/>LED steady ON at 30% brightness]

    H --> I[Screen shows “Start dispensing”<br/>Options: “Take medicine now” / “Set dispensing time”]

    I -->|Select “Take medicine now”| J[Start dispensing immediately]
    I -->|Select “Set dispensing time”| K[Set dispensing time using encoder<br/>Start dispensing when time is reached]

    J --> L[Dispensing in progress]
    K --> L

    L --> M{Pill drop detected successfully?}
    M -->|Yes| N[Message: Dispensing successful]
    M -->|No| O[Message: Dispensing failed<br/>Ask user to check the device]

    N --> P[End of process / Return to initial screen]
    O --> P
```