## Uscite programma e servizio

| Numero Digital_IO | Origine | porta/numero | Note |
| ----------------- | ------- | ------------ | ---- |
| R1                | MH1001  | O6           | `DIGITAL_IO_OUTPUT_RELAY1` |
| R2                | MH1001  | O5           | `DIGITAL_IO_OUTPUT_RELAY2` |
| R3                | MH1001  | O3           | `DIGITAL_IO_OUTPUT_RELAY3` |
| R4                | MH1001  | O4           | `DIGITAL_IO_OUTPUT_RELAY4` |
| LED1              | MH1001  | O1           | `DIGITAL_IO_OUTPUT_WHITE_LED` |
| LED2              | MH1001  | O2           | `DIGITAL_IO_OUTPUT_BLUE_LED` |
| LED3              | MH1001  | O7           | `DIGITAL_IO_OUTPUT_RED_LED` |
| HEATER            | MH1001  | O8           | `DIGITAL_IO_OUTPUT_HEATER1` |
| R5                | MODBUS01| O1           | `OUT09` (coil `relay_start+0`) |
| R6                | MODBUS01| O2           | `OUT10` (coil `relay_start+1`) |
| R7                | MODBUS01| O3           | `OUT11` (coil `relay_start+2`) |
| R8                | MODBUS01| O4           | `OUT12` (coil `relay_start+3`) |
| R9                | MODBUS01| O5           | `OUT13` (coil `relay_start+4`) |
| R10               | MODBUS01| O6           | `OUT14` (coil `relay_start+5`) |
| R11               | MODBUS01| O7           | `OUT15` (coil `relay_start+6`) |
| R12               | MODBUS01| O8           | `OUT16` (coil `relay_start+7`) |

## Ingressi

| Numero Digital_IO | Origine | porta/numero | Note |
| ----------------- | ------- | ------------ | ---- |
| I1                | MH1001  | I6           | `IN06` = `OPTO1` |
| I2                | MH1001  | I5           | `IN05` = `OPTO2` |
| I3                | MH1001  | I8           | `IN08` = `OPTO3` |
| I4                | MH1001  | I7           | `IN07` = `OPTO4` |
| I5                | MH1001  | I1           | `IN01` = `DIP1` |
| I6                | MH1001  | I2           | `IN02` = `DIP2` |
| I7                | MH1001  | I3           | `IN03` = `DIP3` |
| I8                | MH1001  | I4           | `IN04` = `SERVICE_SWITCH` |
| I9                | MODBUS01| I1           | `IN09` (discrete input `input_start+0`) |
| I10               | MODBUS01| I2           | `IN10` (discrete input `input_start+1`) |
| I11               | MODBUS01| I3           | `IN11` (discrete input `input_start+2`) |
| I12               | MODBUS01| I4           | `IN12` (discrete input `input_start+3`) |
| I13               | MODBUS01| I5           | `IN13` (discrete input `input_start+4`) |
| I14               | MODBUS01| I6           | `IN14` (discrete input `input_start+5`) |
| I15               | MODBUS01| I7           | `IN15` (discrete input `input_start+6`) |
| I16               | MODBUS01| I8           | `IN16` (discrete input `input_start+7`) |
