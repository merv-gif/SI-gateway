Solaire Intelligence Gateway Firmware

Firmware for the Solaire Intelligence (SI) inverter gateway, built with ESPHome.

This project provides a lightweight ESP32-based gateway that communicates with Sunsynk / Deye hybrid inverters via RS-485 Modbus and exposes inverter telemetry to Home Assistant.

The gateway forms the core of the Solaire Intelligence optimisation platform, enabling monitoring, forecasting and control of solar energy systems.

⸻

Features

• Direct RS-485 Modbus communication with Sunsynk / Deye inverters
• Fully local operation – no cloud dependency
• Native Home Assistant integration via ESPHome API
• Modular firmware architecture using ESPHome packages
• Hardware abstraction for different gateway boards
• Designed for scalable deployments

⸻

Hardware

The current reference hardware platform is an ESP32 RS-485 development board.

Typical configuration:

Signal | ESP32 Pin
RS485 TX | GPIO17
RS485 RX | GPIO16
RS485 DE / RE | GPIO4

Connection to inverter:

Sunsynk RJ45 | Function
Pin 1 | RS485 B
Pin 2 | RS485 A

Baud rate: 9600

⸻

Repository Structure

SI-gateway
│
├── firmware
│   └── si_gateway.yaml
│
├── packages
│   ├── common.yaml
│   ├── esp32_rs485.yaml
│   └── sunsynk_modbus.yaml
│
└── README.md

⸻

Firmware Structure

firmware/
Contains the main ESPHome firmware configuration used to build the gateway device.

packages/
Reusable ESPHome configuration modules.

File | Purpose
common.yaml | Base configuration (wifi, API, OTA, logging)
esp32_rs485.yaml | Hardware interface for RS485 communication
sunsynk_modbus.yaml | Sunsynk / Deye Modbus register definitions

⸻

Installation
	1.	Install ESPHome in Home Assistant.
	2.	Add the firmware configuration from firmware/si_gateway.yaml.
	3.	Compile and flash the ESP32 device.
	4.	Connect the RS-485 interface to the inverter communication port.

Once online, the gateway will automatically expose inverter sensors to Home Assistant
