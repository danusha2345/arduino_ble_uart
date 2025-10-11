#!/bin/bash

echo "Creating unified firmware files..."

# Create release directory
mkdir -p release

# Function to get file size in hex
get_size_hex() {
    local file="$1"
    if [ -f "$file" ]; then
        local size=$(stat -c%s "$file")
        printf "0x%08X" $size
    else
        echo "0x00000000"
    fi
}

# Function to pad data to 4KB boundary
pad_to_4k() {
    local data="$1"
    local size=$(echo -n "$data" | wc -c)
    local pad_size=$((4096 - size))
    if [ $pad_size -gt 0 ] && [ $pad_size -lt 4096 ]; then
        # Create padding block
        python3 -c "
import struct
pad = b'\xFF' * $pad_size
print(pad)
"
    fi
}

# Create ESP32-C3 unified firmware
echo "Creating ESP32-C3 unified firmware..."
if [ -f ".pio/build/esp32-c3/bootloader.bin" ] && [ -f ".pio/build/esp32-c3/partitions.bin" ] && [ -f ".pio/build/esp32-c3/firmware.bin" ]; then
    # Get sizes
    bootloader_size=$(get_size_hex ".pio/build/esp32-c3/bootloader.bin")
    partitions_size=$(get_size_hex ".pio/build/esp32-c3/partitions.bin")
    firmware_size=$(get_size_hex ".pio/build/esp32-c3/firmware.bin")
    
    # Calculate offsets (standard ESP32 layout)
    bootloader_offset="0x0000"
    partitions_offset="0x8000"
    firmware_offset="0x10000"
    
    # Create unified firmware
    (
        # Add bootloader with padding
        cat .pio/build/esp32-c3/bootloader.bin
        python3 -c "import sys; sys.stdout.buffer.write(b'\xFF' * $((0x8000 - $(stat -c%s .pio/build/esp32-c3/bootloader.bin))))
"
        
        # Add partitions with padding
        cat .pio/build/esp32-c3/partitions.bin
        python3 -c "import sys; sys.stdout.buffer.write(b'\xFF' * $((0x10000 - 0x8000 - $(stat -c%s .pio/build/esp32-c3/partitions.bin))))
"
        
        # Add firmware
        cat .pio/build/esp32-c3/firmware.bin
    ) > release/unified-esp32c3.bin
    
    # Create info file
    cat > release/esp32c3-info.txt << EOF
ESP32-C3 Unified Firmware Information
====================================

Offsets:
- Bootloader:    $bootloader_offset (size: $bootloader_size)
- Partitions:   $partitions_offset (size: $partitions_size) 
- Firmware:     $firmware_offset (size: $firmware_size)

Total size: $(get_size_hex release/unified-esp32c3.bin)

Flashing with esptool:
esptool.py write_flash 0x0 unified-esp32c3.bin

Or flash components separately:
esptool.py write_flash 0x0 .pio/build/esp32-c3/bootloader.bin
esptool.py write_flash 0x8000 .pio/build/esp32-c3/partitions.bin
esptool.py write_flash 0x10000 .pio/build/esp32-c3/firmware.bin
EOF
    
    echo "✓ Created ESP32-C3 unified firmware"
else
    echo "✗ ESP32-C3 files not found"
fi

# Create ESP32-S3 unified firmware  
echo "Creating ESP32-S3 unified firmware..."
if [ -f ".pio/build/esp32-s3/bootloader.bin" ] && [ -f ".pio/build/esp32-s3/partitions.bin" ] && [ -f ".pio/build/esp32-s3/firmware.bin" ]; then
    # Get sizes
    bootloader_size=$(get_size_hex ".pio/build/esp32-s3/bootloader.bin")
    partitions_size=$(get_size_hex ".pio/build/esp32-s3/partitions.bin")
    firmware_size=$(get_size_hex ".pio/build/esp32-s3/firmware.bin")
    
    # Calculate offsets
    bootloader_offset="0x0000"
    partitions_offset="0x8000"
    firmware_offset="0x10000"
    
    # Create unified firmware
    (
        # Add bootloader with padding
        cat .pio/build/esp32-s3/bootloader.bin
        python3 -c "import sys; sys.stdout.buffer.write(b'\xFF' * $((0x8000 - $(stat -c%s .pio/build/esp32-s3/bootloader.bin))))
"
        
        # Add partitions with padding
        cat .pio/build/esp32-s3/partitions.bin
        python3 -c "import sys; sys.stdout.buffer.write(b'\xFF' * $((0x10000 - 0x8000 - $(stat -c%s .pio/build/esp32-s3/partitions.bin))))
"
        
        # Add firmware
        cat .pio/build/esp32-s3/firmware.bin
    ) > release/unified-esp32s3.bin
    
    # Create info file
    cat > release/esp32s3-info.txt << EOF
ESP32-S3 Unified Firmware Information
===================================

Offsets:
- Bootloader:    $bootloader_offset (size: $bootloader_size)
- Partitions:   $partitions_offset (size: $partitions_size)
- Firmware:     $firmware_offset (size: $firmware_size)

Total size: $(get_size_hex release/unified-esp32s3.bin)

Flashing with esptool:
esptool.py write_flash 0x0 unified-esp32s3.bin

Or flash components separately:
esptool.py write_flash 0x0 .pio/build/esp32-s3/bootloader.bin
esptool.py write_flash 0x8000 .pio/build/esp32-s3/partitions.bin
esptool.py write_flash 0x10000 .pio/build/esp32-s3/firmware.bin
EOF
    
    echo "✓ Created ESP32-S3 unified firmware"
else
    echo "✗ ESP32-S3 files not found"
fi

# Also create individual renamed files for compatibility
echo "Creating individual renamed files..."

# ESP32-C3
if [ -f ".pio/build/esp32-c3/firmware.bin" ]; then
    cp .pio/build/esp32-c3/firmware.bin release/firmware-esp32c3.bin
    cp .pio/build/esp32-c3/bootloader.bin release/bootloader-esp32c3.bin
    cp .pio/build/esp32-c3/partitions.bin release/partitions-esp32c3.bin
    echo "✓ Copied ESP32-C3 individual files"
fi

# ESP32-S3
if [ -f ".pio/build/esp32-s3/firmware.bin" ]; then
    cp .pio/build/esp32-s3/firmware.bin release/firmware-esp32s3.bin
    cp .pio/build/esp32-s3/bootloader.bin release/bootloader-esp32s3.bin
    cp .pio/build/esp32-s3/partitions.bin release/partitions-esp32s3.bin
    echo "✓ Copied ESP32-S3 individual files"
fi

# List final files
echo ""
echo "Unified firmware files:"
ls -lh release/unified*.bin 2>/dev/null || echo "No unified files found"

echo ""
echo "Individual firmware files:"
ls -lh release/*-esp32*.bin 2>/dev/null || echo "No individual files found"

echo ""
echo "Info files:"
ls -lh release/*-info.txt 2>/dev/null || echo "No info files found"

echo ""
echo "File sizes comparison:"
echo "ESP32-C3 unified: $(du -h release/unified-esp32c3.bin 2>/dev/null || echo 'Not found')"
echo "ESP32-S3 unified: $(du -h release/unified-esp32s3.bin 2>/dev/null || echo 'Not found')"