# Avocado Zero

Flipper Zero game: maintain an avocado pit in water—clean the glass, grow roots, avoid letting the water get too dirty.

## Requirements

- **Firmware build:** [flipperzero-firmware](https://github.com/flipperdevices/flipperzero-firmware) clone with `./fbt` available, or UFBT with the same app sources.
- **Host tests:** GCC and `make` (no Flipper SDK).

## Build

| Target | Description |
|--------|-------------|
| `make test` | Domain logic unit tests on the host |
| `make prepare` | Symlink this repo into `applications_user/avocado_zero` next to your firmware tree |
| `make fap` | Clean firmware build output and compile the `.fap` (requires `FLIPPER_FIRMWARE_PATH`) |
| `make linter` | `cppcheck` on app sources |
| `make format` | `clang-format` tracked `.c` / `.h` |

Set `FLIPPER_FIRMWARE_PATH` if your firmware is not at `/home/<YOUR_PATCH>/flipperzero-firmware`.

## Save data

`/ext/apps_data/avocado_zero/data.bin`

## License

See `LICENSE`.
