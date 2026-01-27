# Next Steps: Ethernet RX/ARP Regression

## Current state
- Ethernet link, DHCP, and ICMP ping now work when all non‑Ethernet init
  functions are disabled after `start_ethernet()`.
- This confirms the failure is caused by **one of the later peripheral inits**
  interfering with RMII receive or related pins.

## Goal
Identify the specific init call that breaks RX, then fix the pin conflict or
driver behavior while keeping Ethernet stable.

## Plan (one change per build)
Re‑enable one init at a time, rebuild, and test:

1. `init_uart_mdb()`
2. `init_uart_rs485()`
3. `init_uart_rs232()`
4. `init_pwm()`
5. `init_ws2812()`
6. `init_i2c()`
7. `init_boot_button()`
8. `log_sht40_stub()`

After each step:
- Flash
- `ping 192.168.2.25`
- `ip neigh show 192.168.2.25`

The first step that causes `FAILED` in ARP or ping loss is the culprit.

## Expected outcomes
- **Ping still OK** → proceed to next init.
- **Ping fails** → stop and inspect pin usage of that init.

## Typical fixes once the culprit is found
- Pin remap in Kconfig (move conflicting GPIOs away from RMII pins).
- Driver init order adjustment.
- Disable/alter peripheral pin mux for shared pads.

## Notes
- Do not re‑enable multiple inits per build, otherwise the culprit is ambiguous.
- Keep Ethernet init sequence unchanged while testing.

