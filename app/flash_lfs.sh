JLinkExe -CommanderScript flash_lfs.jlink


# !/bin/bash
# set -e
# cd "$(dirname "$0")"

# west build -p always -b nrf54l15dk/nrf54l15/cpuapp .
# west flash
# nrfutil device program \
#     --firmware build/app/lfs.bin \
#     --options address=0x174000 \
#     --options verify=VERIFY_READ

# echo "=== Flash complete ==="