# Differenze comandi CCtalk: implementazione attuale vs standard

Riferimenti:
- Implementazione attuale: `components/cctalk/cctalk.c`
- Standard generico: `docs/cctalk_comandi.md`

| Funzione nel codice | Header usato ora |  | Header standard (`docs/cctalk_comandi.md`) | Differenza |
|---|---:|---:|---|--:|
| `cctalk_modify_master_inhibit()` | **231** | 228 | **103** (`Modify master inhibit status`) | Header diverso |
| `cctalk_modify_inhibit_status()` | **231** | 231 | **134** (`Modify inhibit status`) | Header diverso (conflitto con master inhibit) |
| `cctalk_request_inhibit_status()` | **230** | 230 | **100** (`Request inhibit status`) | Header diverso |
| `cctalk_request_build_code()` | **192** | 192 | **139** (`Request build code`) | Header diverso |
| `cctalk_read_buffered_credit()` | **229** | 229 | **192** (`Read buffered credit or error codes`) | Header diverso |
| `cctalk_request_manufacturer_id()` | **246** | 246 | **231** (`Request manufacturer ID`) | Header diverso |
| `cctalk_request_equipment_category()` | **245** | 245 | **230** (`Request equipment category ID`) | Header diverso |
| `cctalk_request_product_code()` | **244** | 244 | **228** (`Request product code`) | Header diverso |
| `cctalk_request_serial_number()` | **242** | 242 | **229** (`Read serial number`) | Header diverso |

Nota: il mapping attuale è coerente con il profilo specifico documentato in `docs/gettoniera.md` (Microhard), ma differisce dalla tabella standard generica.
