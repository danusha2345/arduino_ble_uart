#!/bin/bash

echo "Preparing firmware files for release..."

# Create release directory
mkdir -p release

# Copy and rename ESP32-C3 files
echo "Processing ESP32-C3 files..."
if [ -f ".pio/build/esp32dev/firmware.bin" ]; then
    cp .pio/build/esp32dev/firmware.bin release/firmware-esp32c3.bin
    echo "✓ Copied ESP32-C3 firmware"
else
    echo "✗ ESP32-C3 firmware not found"
fi

if [ -f ".pio/build/esp32dev/bootloader.bin" ]; then
    cp .pio/build/esp32dev/bootloader.bin release/bootloader-esp32c3.bin
    echo "✓ Copied ESP32-C3 bootloader"
else
    echo "✗ ESP32-C3 bootloader not found"
fi

if [ -f ".pio/build/esp32dev/partitions.bin" ]; then
    cp .pio/build/esp32dev/partitions.bin release/partitions-esp32c3.bin
    echo "✓ Copied ESP32-C3 partitions"
else
    echo "✗ ESP32-C3 partitions not found"
fi

# Copy and rename ESP32-S3 files
echo "Processing ESP32-S3 files..."
if [ -f ".pio/build/esp32s3/firmware.bin" ]; then
    cp .pio/build/esp32s3/firmware.bin release/firmware-esp32s3.bin
    echo "✓ Copied ESP32-S3 firmware"
else
    echo "✗ ESP32-S3 firmware not found"
fi

if [ -f ".pio/build/esp32s3/bootloader.bin" ]; then
    cp .pio/build/esp32s3/bootloader.bin release/bootloader-esp32s3.bin
    echo "✓ Copied ESP32-S3 bootloader"
else
    echo "✗ ESP32-S3 bootloader not found"
fi

if [ -f ".pio/build/esp32s3/partitions.bin" ]; then
    cp .pio/build/esp32s3/partitions.bin release/partitions-esp32s3.bin
    echo "✓ Copied ESP32-S3 partitions"
else
    echo "✗ ESP32-S3 partitions not found"
fi

# List final files
echo ""
echo "Final release files:"
ls -la release/

echo ""
echo "File sizes:"
du -h release/*