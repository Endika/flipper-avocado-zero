# Avocado Zero

Flipper Zero game: maintain an avocado pit in water—clean the glass, grow roots, avoid letting the water get too dirty.

## Requirements

- **Firmware build:** [flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware) clone with `./fbt` available, or UFBT with the same app sources.
- **Host tests:** GCC and `make` (no Flipper SDK).

## Build

| Target | Description |
|--------|-------------|
| `make test` | Domain logic unit tests on the host |
| `make prepare` | Symlink this repo into `applications_user/avocado-zero` next to your firmware tree |
| `make fap` | Clean firmware build output and compile the `.fap` (requires `FLIPPER_FIRMWARE_PATH`) |
| `make linter` | `cppcheck` on app sources |
| `make format` | `clang-format` tracked `.c` / `.h` |

Set `FLIPPER_FIRMWARE_PATH` if your firmware is not at `/home/endika/flipperzero-firmware`.

## Save data

State is stored on the SD card under:

`/ext/apps_data/avocado_zero/data.bin`

Legacy `data.txt` in the same folder is read once and removed after a successful load if present.

## License

See repository `LICENSE` if present; otherwise follow the license chosen by the project author.
