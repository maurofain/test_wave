# Scripts Documentation

This document describes all Bash and Python scripts in the `scripts/` directory and their functionality.

**Total**: 26 scripts (~3,604 lines of code)

---

## Flash & Partition Management Scripts (Bash)

### Hardware Programming

#### `flash_all.sh`
**Purpose**: Flash all firmware partitions to the ESP32-P4 device at once.

**Features**:
- Writes bootloader, partition table, factory app, OTA slots, and SPIFFS storage
- Configurable serial port (default: `/dev/ttyACM0`) and baud rate (default: `2000000`)
- Optional monitor mode (`-m` flag) to view serial output after flashing
- Parameters: `-p <port>`, `-b <baud>`, `-m`

**Flash Layout**:
```
0x2000       → bootloader/bootloader.bin
0x8000       → partition_table/partition-table.bin
0x10000      → build/test_wave_factory.bin (FACTORY)
0x2C0000     → build/test_wave_ota0.bin (OTA_0)
0x4C0000     → build/test_wave_ota1.bin (OTA_1)
0xE80000     → build/storage.bin (SPIFFS)
```

#### `flash_factory.sh`
**Purpose**: Flash only the FACTORY partition (when `COMPILE_APP=0`).

**Features**:
- Validates app version from `app_version.h`
- Supports same options as `flash_all.sh`: `-p`, `-b`, `-m`
- Used for factory/system image flashing
- Writes at offset `0x10000` (4 MB partition size)

#### `flash_ota0.sh`
**Purpose**: Flash only the OTA_0 partition (first OTA slot).

**Features**:
- Partition size: ~4.75 MB at offset `0x4C0000`
- Same command-line options: `-p`, `-b`, `-m`
- Used for OTA (Over-The-Air) updates on primary slot

#### `flash_ota1.sh`
**Purpose**: Flash only the OTA_1 partition (second OTA slot).

**Features**:
- Reads offset from `partitions.csv` for flexibility
- Same command-line options: `-p`, `-b`, `-m`
- Secondary OTA slot for dual-boot redundancy

#### `flash_spiffs.sh`
**Purpose**: Flash only the SPIFFS (file storage) partition.

**Features**:
- Writes filesystem at offset `0xE80000`
- Contains application data, configurations, and embedded assets
- Same command-line options: `-p`, `-b`, `-m`

---

### OTA (Over-The-Air) Updates

#### `flash_factory_ota.sh`
**Purpose**: Upload and activate a firmware binary via HTTP OTA endpoint.

**Features**:
- Sends `build/test_wave.bin` to remote device via POST
- Configurable IP address, custom firmware file, endpoint path
- Dry-run mode (`--dry-run`) to preview upload without executing
- Parameters: `-i/--ip <address>`, `-f/--ota-file <path>`, `-e/--endpoint <path>`, `--dry-run`

**Example**:
```bash
./scripts/flash_factory_ota.sh -i 192.168.1.100 -f build/custom.bin
```

---

### Boot Partition Selection

#### `select_boot_partition.sh`
**Purpose**: Select which firmware partition to boot from (factory or OTA slots).

**Features**:
- `-f`: Boot from FACTORY (erases OTA data for clean factory boot)
- `-0`: Boot from OTA_0
- `-1`: Boot from OTA_1
- Communicates via espefuse to write OTA data
- Parameters: `(-f | -0 | -1)`, `-p <port>`, `-b <baud>`

**Use Case**: Recovery tool when one OTA image is corrupted; forces device to boot specific partition.

#### `switch_to_factory.sh`
**Purpose**: Configure build for FACTORY mode by setting `COMPILE_APP=0`.

**Features**:
- Updates both `main/app_version.h` and `app_version.h`
- Enables factory/system features in conditional compilation
- Called before building factory images

#### `switch_to_production.sh`
**Purpose**: Configure build for PRODUCTION/APP mode by setting `COMPILE_APP=1`.

**Features**:
- Updates both `main/app_version.h` and `app_version.h`
- Enables application-specific features
- Called before building OTA application images

---

## Version & Build Management Scripts

#### `Version_bundle.sh`
**Purpose**: Archive and version build artifacts with metadata.

**Features**:
- Reads `APP_VERSION` and `APP_DATE` from `app_version.h`
- Extracts `COMPILE_APP` flag (0=factory, 1=app)
- Creates timestamped directory: `versions/<mode>_v<version>_<date>/`
- Bundles:
  - All `.bin` files from `build/`
  - Full `build/` directory tree
  - Optional core dump file (passed as parameter)
- Useful for release preparation and artifact management

**Usage**:
```bash
./scripts/Version_bundle.sh [coredump_file]
```

#### `sync_to_mh1001.sh`
**Purpose**: Synchronize current project to a parallel `mh1001` repository.

**Features**:
- Copies select files/directories to `../mh1001/`
- Dry-run mode (`--dry-run`, `-n`) shows what would be copied without executing
- Used for mirroring changes to a related project variant
- Parameters: `[--dry-run]`, `[--help]`

---

## Documentation Generation

#### ✔️`generate_doxygen.sh`
**Purpose**: Generate Doxygen-based C code documentation with graphs.

**Features**:
- Reads configuration from `Doxyfile`
- Outputs to `docs/doxygen/` directory
- Generates both HTML and optional Graphviz diagrams
- Warns if graphviz (`dot`) not installed, allows markdown generation without graphs
- Copies generated markdown files for Git tracking

**Requirements**:
- `doxygen` (required)
- `graphviz` (optional, for call/class graphs)

#### `add_doxygen_templates.sh`
**Purpose**: Auto-insert Doxygen comment skeletons above function definitions.

**Features**:
- Scans all `.c` files for function definitions
- Detects functions lacking Doxygen comments (`/** ... */`)
- Inserts template with:
  - `@brief` placeholder
  - `@param [in]`/`@param [out]` templates
  - `@return` template
- Preserves existing comments, only fills gaps

**Output**: Modified `.c` files with placeholder comments for manual completion.

---

## Internationalization (i18n) Scripts (Python)

#### 🚫`generate_language_models.py`
**Purpose**: Generate `language_models.h` C header from localized JSON files.

**Features**:

- Converts `data/i18n_<lang>.json` → C structs in `language_models.h`
- Supports languages: Italian (it), English (en), German (de), French (fr), Spanish (es)
- Maps JSON string IDs → C array indices
- Fallback logic: en → it → hardcoded glossary for missing translations
- Includes legacy literal IDs for backward compatibility

**Input**: `data/i18n_<lang>.json` and `data/i18n_<lang>.map.json`

**Output**: `components/lvgl_panel/language_models.h` (C header with embedded strings)

#### ✔️`generate_i18n_json_from_header.py`
**Purpose**: Reverse-extract localized strings from `language_models.h` back to JSON.

**Features**:
- Parses C header for embedded string data
- Reconstructs `data/i18n_<lang>.json` and map files
- Part of bidirectional workflow: JSON ↔ C header
- Validates against backup (`.bak`) files for consistency

**Use Case**: Audit and version control of translated strings; ensures round-trip fidelity.

#### `convert_i18n_compact.py`
**Purpose**: Compress i18n JSON to compact format with size constraints.

**Features**:
- Splits long strings into 31-byte UTF-8 chunks (+ null terminator = 32 bytes)
- Optimal for embedded systems with limited memory
- Compacts text by breaking long messages across multiple array slots
- Output: `data/i18n_<lang>_compact.json`

**Constraint**: Max text size is 31 bytes to fit in 32-byte aligned buffers.

#### `generate_i18n_sync.py`
**Purpose**: Synchronize i18n records from backup files to sync JSON.

**Features**:
- Processes all `data/i18n_*.json.bak` files
- Exports to `data/i18n_<lang>_sync.json`
- Organizes records by scope (UI sections, menus, etc.)
- Quick data extraction/backup utility

#### `compare_i18n.py`
**Purpose**: Validate i18n translation quality by comparing original vs. reconstructed strings.

**Features**:
- Loads backup (`.bak`) and compact format files
- Generates difference reports: `data/i18n_compare_<lang>.md`
- Detects string loss, encoding issues, or truncation bugs
- Useful for QA and migration validation

**Output**: Markdown reports with side-by-side diffs for mismatches.

#### `remove_lang_from_i18n.py`
**Purpose**: Clean up language files by removing unused keys.

**Features**:
- Backs up current JSON to `.json.bak` if not already backed up
- Removes language-specific fields or keys from i18n records
- Helps maintain consistent i18n structure across updates

---

## Image & UI Generation

#### ✔️`generate_language_flags.py`
**Purpose**: Convert PNG flag images to embedded C byte arrays in LVGL format.

**Features**:
- Reads flag PNG files: `flag_IT.png`, `flag_EN.png`, etc.
- Converts to LV_COLOR_FORMAT_RGB565A8 (LVGL v9 format)
- Layout: planar RGB565 (little-endian) + alpha channel
- Outputs: `components/lvgl_panel/language_flags.c`
- Stride: `FLAG_WIDTH * 2` bytes for color plane
- Total size: `FLAG_WIDTH × FLAG_HEIGHT × 3` bytes (per flag)

**Use Case**: Language selection UI with visual flag icons in LVGL.

#### ✔️`generate_slideshow_images.py`
**Purpose**: Process and optimize marketing/promotional images for slideshow display.

**Features**:
- Reads source images from `docs/images/`
- Crops and resizes to 692×904 pixels (3:4 aspect ratio)
- Outputs JPEG with quality 85 to `data/`
- Optimized for device display screens
- Removes hardcoded paths in script (requires editing source paths)

**Use Case**: Generate on-device promotional images/slideshows.

---

## Utility & Testing Scripts

#### `find_c_files.py`
**Purpose**: Analyze C codebase structure and generate reports.

**Features**:
- Scans all `.c` files in current directory recursively
- Counts lines of code (LOC) per file
- Detects disabled code blocks (`#if 0 ... #endif`)
- Generates markdown reports:
  - `scripts/c_files_report.md`: File list with LOC (descending)
  - `scripts/c_functions_report.md`: Function inventory
- Supports CLI options: `--sort` (asc/desc), `--format` (md/csv)

**Use Case**: Codebase metrics, dead code detection, documentation generation.

#### ✔️`export_embedded_pages.py`
**Purpose**: Extract embedded HTML/CSS from C source and export as standalone web pages.

**Features**:
- Parses C source for `const char *` declarations containing HTML
- Reconstructs complete HTML pages with embedded CSS/JavaScript
- Handles style injection from base template
- Detects `<img>` references and relocates to `data/` directory
- Outputs standalone `.html` files for review/testing
- Supports command-line filtering: `--filter <string>` to export specific pages

**Use Case**: QA testing of embedded web UI; audit embedded HTML strings.

#### `login_test.py`
**Purpose**: Test authentication endpoint with exact firmware parameters.

**Features**:
- Sends POST request to `/api/login` with:
  - Serial number (default: `AD-34-DFG-333`)
  - MD5 password hash (default: hardcoded from config)
  - Date header (default: project hardcoded date `2026-01-23T13:25:13.218763+01:00`)
  - Base URL (default: `http://195.231.69.227:5556/`)
- Logs all requests and responses to `login_test.log`
- Verbose console output for debugging
- Parameters: `--url`, `--serial`, `--password`

**Use Case**: Integration testing, auth backend validation, network troubleshooting.

**Example**:
```bash
python3 scripts/login_test.py --url http://localhost:5556 --serial MY-SERIAL
```

#### ✔️`dgen.py`
**Purpose**: Auto-generate Doxygen comments in Italian using Ollama AI.

**Features**:
- Integrates with local Ollama instance (default: `http://127.0.0.1:11434`)
- Uses model: `qwen2.5-coder:7b`
- Parses function signatures and generates Italian documentation
- Tracks progress: `[index/total]` display during generation
- Output: Doxygen-formatted comments ready to insert in source

**Requirements**:
- Ollama running locally with qwen2.5-coder model
- Internet optional (runs locally)

**Use Case**: Bulk Doxygen comment generation for Italian codebase.

---

## Summary by Category

| Category | Scripts | Purpose |
|----------|---------|---------|
| **Flash Operations** | 5 scripts | Bootloader/firmware flashing to ESP32-P4 |
| **OTA Updates** | 1 script | HTTP-based firmware updates |
| **Boot Management** | 3 scripts | Partition selection and compilation mode |
| **Version/Build** | 2 scripts | Release bundling and project sync |
| **Documentation** | 2 scripts | Doxygen generation and comment templates |
| **Internationalization** | 6 scripts | i18n workflow, translation management |
| **UI/Images** | 2 scripts | Flag icons and slideshow optimization |
| **Utilities** | 5 scripts | Code analysis, testing, HTML export |

---

## Typical Workflows

### Build & Flash Factory Image
```bash
./scripts/switch_to_factory.sh
idf.py build
./scripts/flash_factory.sh -p /dev/ttyACM0
```

### Build & Flash OTA Update
```bash
./scripts/switch_to_production.sh
idf.py build
./scripts/flash_ota0.sh -p /dev/ttyACM0
./scripts/select_boot_partition.sh -0 -p /dev/ttyACM0
```

### Release Versioning
```bash
./scripts/Version_bundle.sh
# Creates: versions/<mode>_v<version>_<date>/
```

### Update UI Translations
```bash
# Edit translations in data/i18n_<lang>.json
python3 scripts/generate_language_models.py
idf.py build
```

### Generate Documentation
```bash
./scripts/add_doxygen_templates.sh  # Add skeleton comments
dgen.py                            # Auto-generate Italian docs
./scripts/generate_doxygen.sh      # Build HTML docs
```

---

**Document Version**: 1.0  
**Generated**: 2026-03-12  
**Repository**: test_wave (ESP32-P4 Firmware Project)
