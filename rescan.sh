#!/bin/bash
# Remove the old device (if it was there before)
echo 1 > /sys/bus/pci/devices/0000\:02\:00.0/remove  # Replace XX with your actual PCIe bus ID

# Force a rescan of the PCIe bus
echo 1 > /sys/bus/pci/rescan
