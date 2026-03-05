# ModBus.md

## Waveshare Modbus RTU Relay (D) -- SKU 26517

Technical reference and ESP‑IDF usage guide

------------------------------------------------------------------------

# 1. Overview

The **Waveshare Modbus RTU Relay (D)** module is an industrial relay
controller with:

-   8 electromechanical relays (SPDT)
-   8 opto‑isolated digital inputs
-   RS485 communication
-   Modbus RTU protocol
-   Wide power supply (7--36V DC)
-   DIN rail industrial enclosure

The device acts as a **Modbus RTU slave** and can be controlled by:

-   PLC
-   industrial PC
-   Linux system
-   microcontrollers such as **ESP32**
-   SCADA systems

Typical use cases:

-   industrial automation
-   remote power switching
-   building automation
-   data acquisition
-   industrial IoT gateways

------------------------------------------------------------------------

# 2. Electrical Specifications

## Power

  Parameter         Value
  ----------------- -----------
  Supply voltage    7--36V DC
  Typical supply    12V / 24V
  Logic isolation   yes

## Relays

  Parameter   Value
  ----------- --------------
  Channels    8
  Type        SPDT
  Max load    10A @ 250VAC
  Max load    10A @ 30VDC

Each relay terminal:

COM -- common\
NO -- normally open\
NC -- normally closed

## Digital Inputs

  Parameter            Value
  -------------------- -------------
  Channels             8
  Input voltage        5--36V
  Isolation            optocoupler
  Compatible sensors   PNP / NPN

------------------------------------------------------------------------

# 3. Communication Interface

## RS485

Differential bus used for industrial communication.

Terminals:

A → RS485 A\
B → RS485 B\
GND → optional reference

Recommended:

-   twisted pair cable
-   termination resistor (120Ω) at bus ends

## Default Serial Parameters

  Parameter   Value
  ----------- ------------
  Baudrate    9600
  Data bits   8
  Parity      None
  Stop bits   1
  Mode        Modbus RTU

Device address range:

1 -- 255

------------------------------------------------------------------------

# 4. Modbus Register Map

## Relay Control (Coils)

  Address   Function
  --------- ----------
  0000      Relay 1
  0001      Relay 2
  0002      Relay 3
  0003      Relay 4
  0004      Relay 5
  0005      Relay 6
  0006      Relay 7
  0007      Relay 8

Write:

-   0 → OFF
-   1 → ON

Supported function codes:

  Code   Description
  ------ ----------------------
  0x01   Read Coils
  0x05   Write Single Coil
  0x0F   Write Multiple Coils

------------------------------------------------------------------------

## Digital Inputs (Discrete Inputs)

  Address   Input
  --------- -------
  0000      DI1
  0001      DI2
  0002      DI3
  0003      DI4
  0004      DI5
  0005      DI6
  0006      DI7
  0007      DI8

Supported function code:

0x02 -- Read Discrete Inputs

------------------------------------------------------------------------

# 5. Typical RS485 Network

Example topology:

ESP32 (Master) \| RS485 Transceiver (MAX3485 / SP3485) \| RS485 BUS \|
Modbus Relay Module

Multiple modules can share the same bus with different addresses.

------------------------------------------------------------------------

# 6. ESP32 Integration (ESP‑IDF)

ESP‑IDF includes a **native Modbus controller component**.

Component:

mbcontroller

Protocol supported:

-   Modbus RTU
-   Modbus ASCII
-   Modbus TCP

------------------------------------------------------------------------

# 7. Hardware Connection (ESP32)

Example using MAX3485.

ESP32 → RS485

  ESP32   RS485
  ------- -------
  TXD     DI
  RXD     RO
  GPIO    RE/DE
  GND     GND

Typical UART:

UART2

------------------------------------------------------------------------

# 8. ESP‑IDF Initialization Example

``` c
#include "mbcontroller.h"
#include "esp_log.h"

#define MB_UART_PORT UART_NUM_2
#define MB_DEVICE_ADDR 1

void modbus_init()
{
    void* handler = NULL;

    mb_communication_info_t comm = {
        .mode = MB_MODE_RTU,
        .port = MB_UART_PORT,
        .baudrate = 9600,
        .parity = MB_PARITY_NONE
    };

    mbcontroller_init(MB_PORT_SERIAL_MASTER, &handler);

    mbcontroller_setup((void*)&comm);

    mbcontroller_start();

    ESP_LOGI("MODBUS","Modbus master started");
}
```

------------------------------------------------------------------------

# 9. Relay Control Example

Turn ON relay 1.

``` c
uint16_t coil_addr = 0;
uint8_t state = 1;

mb_param_request_t req = {
    .slave_addr = 1,
    .command = MB_FUNC_WRITE_SINGLE_COIL,
    .reg_start = coil_addr,
    .reg_size = 1
};

mbcontroller_send_request(&req, &state);
```

Turn OFF relay 1.

``` c
state = 0;
mbcontroller_send_request(&req, &state);
```

------------------------------------------------------------------------

# 10. Read Digital Inputs

``` c
uint8_t inputs[8];

mb_param_request_t req = {
    .slave_addr = 1,
    .command = MB_FUNC_READ_DISCRETE_INPUTS,
    .reg_start = 0,
    .reg_size = 8
};

mbcontroller_send_request(&req, inputs);
```

Result:

inputs\[i\] =

0 → input LOW\
1 → input HIGH

------------------------------------------------------------------------

# 11. Control Multiple Relays

Example turning ON relay 1--4.

``` c
uint8_t states[4] = {1,1,1,1};

mb_param_request_t req = {
    .slave_addr = 1,
    .command = MB_FUNC_WRITE_MULTIPLE_COILS,
    .reg_start = 0,
    .reg_size = 4
};

mbcontroller_send_request(&req, states);
```

------------------------------------------------------------------------

# 12. Example Application Logic

Example automation loop.

``` c
while(1)
{
    read_inputs();

    if(inputs[0])
        relay_on(0);
    else
        relay_off(0);

    vTaskDelay(pdMS_TO_TICKS(100));
}
```

Possible uses:

-   sensor triggered relay
-   industrial alarm systems
-   energy control

------------------------------------------------------------------------

# 13. Performance Notes

RS485 max cable:

-   up to 1200m (low speed)

Typical industrial values:

-   9600 → long distance
-   115200 → short distance

Polling strategy recommended:

50--200 ms

------------------------------------------------------------------------

# 14. Best Practices

Recommended:

-   add RS485 termination
-   use shielded cable
-   avoid star topology
-   use unique Modbus IDs
-   isolate industrial power lines

------------------------------------------------------------------------

# 15. Useful Tools

PC Modbus testing software:

-   Modbus Poll
-   QModMaster
-   SimplyModbus

USB adapters:

-   USB‑RS485 FTDI
-   CH340 RS485 converters

------------------------------------------------------------------------

# 16. Summary

The Waveshare Modbus RTU Relay module provides:

-   reliable industrial relay control
-   simple Modbus protocol
-   robust RS485 communication
-   easy integration with ESP32 using ESP‑IDF

It is well suited for:

-   industrial automation
-   remote switching
-   embedded control systems
