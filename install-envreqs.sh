#!/bin/bash
echo "---------- start install requirements ----------"
set -e
set -x

if [ $EUID == 0 ]; then
    echo "Please do not run this script as root; don't sudo it!"
    exit 1
fi

# usage() {
#     echo "Usage: $0 [--ardu-conda ENV_NAME], only used for ardupilot"
#     exit 1
# }

# ARDU_ENV=""

# while [[ "$#" -gt 0 ]]; do
#     case $1 in
#         --ardu-conda)
#             ARDU_ENV="$2"
#             shift 2
#             ;;
#         *)
#             echo "Unknown parameter passed: $1"
#             usage
#             ;;
#     esac
# done

APT_GET="sudo apt-get -y"

# update apt package list
$APT_GET update

## install deps' requirements
$APT_GET install libgmp-dev libmpfr-dev texinfo bison ninja-build g++-aarch64-linux-gnu gcc-aarch64-linux-gnu g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf minisat libgoogle-perftools-dev python3.10-venv protobuf-compiler libprotobuf-dev nlohmann-json3-dev libgtk-3-dev libgtk2.0-dev libffi-dev

$APT_GET install python3 libzstd-dev libtinfo-dev zlib1g-dev cvc4 libcvc4-dev libcln-dev flex python3-dev graphviz libgraphviz-dev pkg-config python3-pip gcc-12 g++-12 openjdk-17-jdk zip unzip libboost-all-dev librapidxml-dev libyaml-cpp-dev python3-colcon-common-extensions   

pip3 install -r "$(dirname "$0")/requirements.txt"
sudo pip3 install cmake==3.30.2

# add gcc-12 and g++-12 to alternatives
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 70
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 70

# set gcc and g++ to gcc-12 and g++-12
sudo update-alternatives --set gcc /usr/bin/gcc-12
sudo update-alternatives --set g++ /usr/bin/g++-12

## install ROS2's requirements
$APT_GET update && $APT_GET install locales
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8
$APT_GET install software-properties-common
sudo add-apt-repository universe
$APT_GET update && $APT_GET install curl gnupg lsb-release
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(source /etc/os-release && echo $UBUNTU_CODENAME) main" | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

$APT_GET update
$APT_GET install ros-humble-desktop
$APT_GET install python3-argcomplete

source /opt/ros/humble/setup.bash
echo " source /opt/ros/humble/setup.bash" >> ~/.bashrc 

## install MAVROS's requirements
$APT_GET install ros-humble-mavros ros-humble-mavros-extras
wget https://raw.githubusercontent.com/mavlink/mavros/master/mavros/scripts/install_geographiclib_datasets.sh
sudo bash ./install_geographiclib_datasets.sh
rm ./install_geographiclib_datasets.sh

# # install ardupilot's requirements
# if [ -n "$ARDU_ENV" ]; then
#     eval "$(conda shell.bash hook)"
#     conda activate $ARDU_ENV
#     echo "current conda env: $CONDA_DEFAULT_ENV"
# fi
# current_dir=$(pwd)
# atdupilot_dir=$current_dir"/flight-control/arducopter-4.4"
# # check if ardupilot is cloned
# if [ ! -d $atdupilot_dir ]; then
#     echo "Please execute the following command in project root directory to clone ardupilot:"
#     echo "root directory: $current_dir" 
#     echo "command: git submodule update --init --recursive"
#     exit 1
# fi
# cd $atdupilot_dir
# Tools/environment_install/install-prereqs-ubuntu.sh -y
# cd $current_dir

echo "---------- install end ----------"
