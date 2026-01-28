#!/bin/bash

# Display help information
show_help() {
    echo "Usage: $0 -t <flight_control_type>"
    echo ""
    echo "Install development environment for specified flight control system."
    echo ""
    echo "Options:"
    echo "  -t, --type <ardupilot|px4|paparazzi>  Specify the flight control type to install"
    echo "  -h, --help                            Display this help message and exit"
    echo ""
    echo "Examples:"
    echo "  $0 -t ardupilot     # Install ArduPilot environment"
    echo "  $0 -t px4           # Install PX4 environment"
    echo "  $0 -t paparazzi     # Install Paparazzi environment"
    echo ""
    exit 1
}

# Install ArduPilot environment
install_ardupilot() {
    echo "--- Installing ArduPilot environment ---"
    if [ -d "$ARDUPILOT_DIR" ]; then
        cd "$ARDUPILOT_DIR"
        bash Tools/environment_install/install-prereqs-ubuntu.sh -y
        echo "ArduPilot environment installation completed"
    else
        echo "Error: ArduPilot directory does not exist: $ARDUPILOT_DIR"
        echo "Please clone the ArduPilot repository or check the directory path"
        exit 1
    fi
}

# Install PX4 environment
install_px4() {
    echo "--- Installing PX4 environment ---"
    if [ -d "$PX4_DIR" ]; then
        cd "$PX4_DIR"
        bash Tools/setup/ubuntu.sh
        echo "PX4 environment installation completed"
    else
        echo "Error: PX4 directory does not exist: $PX4_DIR"
        echo "Please clone the PX4 repository or check the directory path"
        exit 1
    fi
}

# Install Paparazzi environment
install_paparazzi() {
    echo "--- Installing Paparazzi environment ---"
    if [ -d "$PAPARAZZI_DIR" ]; then
        # Add Paparazzi environment installation commands here
        echo "Paparazzi environment installation completed"
    else
        echo "Error: Paparazzi directory does not exist: $PAPARAZZI_DIR"
        echo "Please clone the Paparazzi repository or check the directory path"
        exit 1
    fi
}

# Set project paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

ARDUPILOT_DIR="$PROJECT_ROOT/flight-control/arducopter-4.4"
PX4_DIR="$PROJECT_ROOT/flight-control/PX4-1.15.2"
PAPARAZZI_DIR="$PROJECT_ROOT/flight-control"

# Initialize variables
install_type=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -t|--type)
            if [ -z "$2" ] || [[ "$2" == -* ]]; then
                echo "Error: -t option requires an argument (ardupilot|px4|paparazzi)"
                show_help
            fi
            install_type="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            ;;
        *)
            echo "Error: Unknown option $1"
            show_help
            ;;
    esac
done

# Handle installation logic
if [ -n "$install_type" ]; then
    # Install specified type environment
    case "$install_type" in
        ardupilot)
            install_ardupilot
            ;;
        px4)
            install_px4
            ;;
        paparazzi)
            install_paparazzi
            ;;
        *)
            echo "Error: Invalid flight control type '$install_type'"
            echo "Valid types: ardupilot, px4, paparazzi"
            show_help
            ;;
    esac
else
    # No parameters specified
    echo "Error: No flight control type specified"
    show_help
fi