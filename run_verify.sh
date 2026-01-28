#!/bin/bash

#abort on error
set -e

# Global variable to store the script purpose
script_purpose=""

# Array to store positional arguments
positional_args=()

# Function to display usage
usage() {
    echo "usage: run_verify.sh -u <purpose> [positional_args...]"
    echo ""
    echo "Execute different tasks based on the specified purpose."
    echo ""
    echo "Options:"
    echo "  -u <purpose> | --use <purpose> : Specify the purpose of the script execution."
    echo "                                   Valid purposes: gen_ir, build_uavs, specify_property, slice, gen_uppaal_model, instrument, ir_build"
    echo "                                   run_sim, gen_temps, create_input_seeds, instrument_klee_entry, klee, create_inputs, vectorize, find_slice_criterion, download_pi, verify"
    echo "  -h | --help                    : Display this help message."
    echo ""
    echo "Purposes:"
    echo "  gen_ir       : Generate IR (Intermediate Representation) for verification."
    echo "                 Usage: -u gen_ir <ardupilot|px4> [nop]"
    echo "                 The 'nop' parameter disables opaque pointers in the IR generation."
    echo "                 Example: run_verify.sh -u gen_ir ardupilot"
    echo ""
    echo "  build_uavs   : Build all available UAV softwares found in flight-control directory."
    echo "                 Usage: -u build_uavs [nop] [-cn <conda_env>]"
    echo "                 The 'nop' parameter disables opaque pointers during the build process."
    echo "                 The '-cn <conda_env>' parameter specifies conda environment for the build."
    echo "                 Example: run_verify.sh -u build_uavs nop"
    echo "                 Example: run_verify.sh -u build_uavs -cn myenv"
    echo "                 Example: run_verify.sh -u build_uavs nop -cn myenv"
    echo ""
    echo "  specify_property: Create IR configuration file for property verification."
    echo "                 Usage: -u specify_property <property_name>"
    echo "                 Example: run_verify.sh -u specify_property property_id"
    echo ""
    echo "  slice        : Generate function level slices from LLVM IR file."
    echo "                 Usage: -u slice"
    echo "                 Example: run_verify.sh -u slice"
    echo ""
    echo "  gen_uppaal_model : Generate UPPAAL model files."
    echo "                     Usage: -u gen_uppaal_model"
    echo "                     Example: run_verify.sh -u gen_uppaal_model"
    echo ""
    echo "  instrument   : Instrument the LLVM IR for data collection."
    echo "                 Usage: -u instrument"
    echo "                 Example: run_verify.sh -u instrument"
    echo ""
    echo "  ir_build     : Build IR with configuration file."
    echo "                 Usage: -u ir_build"
    echo "                 Example: run_verify.sh -u ir_build"
    echo ""
    echo "  gen_temps    : Generate template files using MTGen."
    echo "                 Usage: -u gen_temps"
    echo "                 Example: run_verify.sh -u gen_temps"
    echo ""
    echo "  run_sim: Run data collector for the model."
    echo "                 Usage: -u run_sim"
    echo "                 Example: run_verify.sh -u run_sim"
    echo ""
    echo "  create_input_seeds: Create model input XML files from binary format data."
    echo "                 Usage: -u create_input_seeds"
    echo "                 Example: run_verify.sh -u create_input_seeds"
    echo ""
    echo "  instrument_klee_entry: Instrument KLEE entry function."
    echo "                 Usage: -u instrument_klee_entry"
    echo "                 Example: run_verify.sh -u instrument_klee_entry"
    echo ""
    echo "  klee: Perform symbolic execution with KLEE."
    echo "                 Usage: -u klee"
    echo "                 Example: run_verify.sh -u klee"
    echo ""
    echo "  create_inputs: Convert .ktest files to model input .mi files."
    echo "                 Usage: -u create_inputs"
    echo "                 Example: run_verify.sh -u create_inputs"
    echo ""
    echo "  vectorize    : Vectorize the flight control code for RAG system."
    echo "                 Usage: -u vectorize <ArduCopter|PX4>"
    echo "                 Example: run_verify.sh -u vectorize ArduCopter"
    echo ""
    echo "  find_slice_criterion : Find slice criterion using LLM and RAG system."
    echo "                 Usage: -u find_slice_criterion"
    echo "                 Example: run_verify.sh -u find_slice_criterion"
    echo ""
    echo "  upload_pi    : Upload flight control software to Raspberry Pi."
    echo "                 Usage: -u upload_pi"
    echo "                 Example: run_verify.sh -u upload_pi"
    echo ""
    echo "  download_pi  : Download dumped data from Raspberry Pi."
    echo "                 Usage: -u download_pi"
    echo "                 Example: run_verify.sh -u download_pi"
    echo ""
    echo "  run_uav      : Run UAV simulation with optional time instrumentation."
    echo "                 Usage: -u run_uav [--isTime]"
    echo "                 The '--isTime' flag enables time instrumentation for the simulation."
    echo "                 Example: run_verify.sh -u run_uav"
    echo "                 Example: run_verify.sh -u run_uav --isTime"
    echo ""
    echo "  verify       : Execute UPPAAL model verification."
    echo "                 Usage: -u verify [--threads <number>]"
    echo "                 The '--threads <number>' option specifies the number of parallel verification threads."
    echo "                 If not specified, the thread count defaults to CPU core count."
    echo "                 Example: run_verify.sh -u verify"
    echo "                 Example: run_verify.sh -u verify --threads 8"
    echo ""
    echo "Note: When using both 'gen_ir' and 'build_uavs', the 'nop' option must be consistently"
    echo "      enabled or disabled for both commands to ensure compatibility."
    echo ""
    echo "Positional Arguments:"
    echo "  Any arguments not preceded by -u or --use will be treated as positional."
    echo "  The interpretation depends on the specified purpose."
    exit 1
}

# Function to parse command-line arguments
parse_args() {
    # Reset positional args array
    positional_args=()

    while [[ $# -gt 0 ]]; do
        case "$1" in
            -u | --use )
                if [ -z "$2" ]; then
                    echo "Error: Option $1 requires an argument specifying the purpose." >&2
                    usage
                fi
                script_purpose="$2"
                shift # Consume the option (-u or --use)
                shift # Consume the option's argument (<purpose>)
                ;;

            -h | --help )
                usage
                exit 0
                ;;
            * )
                # If it's not a known option, treat it as a positional argument
                positional_args+=("$1")
                shift # Consume the positional argument
                ;;
        esac
    done

    # Validate that a purpose was specified
    if [ -z "$script_purpose" ]; then
        echo "Error: No purpose specified. Use -u or --use option." >&2
        usage
    fi

    # Validate the purpose
    case "$script_purpose" in
        "gen_ir"|"build_uavs"|"specify_property"|"slice"|"gen_uppaal_model"|"instrument"|"ir_build"|"gen_temps"|"run_sim"|"create_input_seeds"|"instrument_klee_entry"|"klee"|"create_inputs"|"vectorize"|"find_slice_criterion"|"upload_pi"|"download_pi"|"run_uav"|"verify")
            # Valid purpose
            ;;
        *)
            echo "Error: Invalid purpose '$script_purpose'. Valid purposes are: gen_ir, build_uavs, specify_property, slice, gen_uppaal_model, instrument, ir_build, gen_temps, run_sim, create_input_seeds, instrument_klee_entry, klee, create_inputs, vectorize, find_slice_criterion, upload_pi, download_pi, run_uav, verify." >&2
            usage
            ;;
    esac
}

build_uavs() {
    echo "Building all available UAV softwares..."
    
    # Get current directory
    CURRENT_DIR=$(pwd)
    
    # Initialize flags
    NOP_FLAG=""
    CONDA_FLAG=""
    
    # Parse positional arguments for build_uavs
    i=0
    while [ $i -lt ${#positional_args[@]} ]; do
        case "${positional_args[$i]}" in
            "nop")
                NOP_FLAG="-nop"
                echo "Building with NOP (No Opaque Pointers) option enabled"
                ;;
            "-cn")
                if [ $((i+1)) -lt ${#positional_args[@]} ]; then
                    CONDA_FLAG="${positional_args[$((i+1))]}"
                    echo "Using conda environment: ${positional_args[$((i+1))]}"
                    i=$((i+1))  # Skip the next argument as it's the conda env name
                else
                    echo "Error: -cn flag requires a conda environment name" >&2
                    exit 1
                fi
                ;;
        esac
        i=$((i+1))
    done
    
    # Check and build px4 if directory exists
    if [ -d "${CURRENT_DIR}/flight-control/PX4-1.15.2" ]; then
        echo "Building PX4 model..."
        if [ -n "$CONDA_FLAG" ]; then
            python3 verify-api.py uav_build -t px4 -cn ${CONDA_FLAG} ${NOP_FLAG} -m
        else
            python3 verify-api.py uav_build -t px4 ${NOP_FLAG} -m
        fi
    else
        echo "Skipping PX4 build: directory not found"
    fi
    
    # Check and build paparazzi if directory exists
    if [ -d "${CURRENT_DIR}/flight-control/paparazzi" ]; then
        echo "Building Paparazzi model..."
        if [ -n "$CONDA_FLAG" ]; then
            python3 verify-api.py uav_build -t paparazzi -cn ${CONDA_FLAG} ${NOP_FLAG} -m
        else
            python3 verify-api.py uav_build -t paparazzi ${NOP_FLAG} -m
        fi
    else
        echo "Skipping Paparazzi build: directory not found"
    fi
    
    # Check and build ardupilot if directory exists
    if [ -d "${CURRENT_DIR}/flight-control/arducopter-4.4" ]; then
        echo "Building ArduPilot model..."
        if [ -n "$CONDA_FLAG" ]; then
            python3 verify-api.py uav_build -t ardupilot -cn ${CONDA_FLAG} ${NOP_FLAG} -m
        else
            python3 verify-api.py uav_build -t ardupilot ${NOP_FLAG} -m
        fi
    else
        echo "Skipping ArduPilot build: directory not found"
    fi
}

gen_ir() {
    echo "Generating IR for verification..."
    # Check if we have at least one positional argument
    if [ ${#positional_args[@]} -lt 1 ]; then
        echo "Error: gen_ir requires at least 1 positional argument." >&2
        echo "Usage: run_verify.sh -u gen_ir <ardupilot|px4> [nop]" >&2
        exit 1
    fi
    # Validate the first argument
    case "${positional_args[0]}" in
        "ardupilot"|"px4")
            # Valid first argument
            ;;
        *)
            echo "Error: First argument must be one of: ardupilot, px4" >&2
            echo "Usage: run_verify.sh -u gen_ir <ardupilot|px4> [nop]" >&2
            exit 1
            ;;
    esac
    # Validate the second argument if present
    if [ ${#positional_args[@]} -gt 1 ]; then
        if [ "${positional_args[1]}" != "nop" ]; then
            echo "Error: Second argument if provided must be 'nop'" >&2
            echo "Usage: run_verify.sh -u gen_ir <ardupilot|px4> [nop]" >&2
            exit 1
        fi
        # If second argument exists and is valid, pass it as a flag to verify-api.py
        python3 verify-api.py uav_ir -t "${positional_args[0]}" --no-opaque-pointers
    else
        # If no second argument, call without it
        python3 verify-api.py uav_ir -t "${positional_args[0]}"
    fi

    CURRENT_DIR=$(pwd)
    if [ "${positional_args[0]}" = "ardupilot" ]; then
        echo "Generating IR for ardupilot..."
        ${CURRENT_DIR}/preAnalyzer/build/inCallAnalysis -v
        ${CURRENT_DIR}/preAnalyzer/build/addTargetsMD merge --dir ${CURRENT_DIR}/preAnalyzer/build/incalls --manual ${CURRENT_DIR}/configs/arduInCallsAdd.yml
        mv indirectCalls.yml ${CURRENT_DIR}/configs/arduInCalls.yml
        ${CURRENT_DIR}/preAnalyzer/build/addTargetsMD add-md ${CURRENT_DIR}/verifyDataBase/ir_and_elf/arducopter_sitl_pi64.ll --incall-config ${CURRENT_DIR}/configs/arduInCalls.yml --dma-config ${CURRENT_DIR}/configs/arduDMATypeAdd.yml
    
    elif [ "${positional_args[0]}" = "px4" ]; then
        echo "Generating IR for px4..." 
        # TODO: Add px4-specific additional commands here
        ${CURRENT_DIR}/preAnalyzer/build/addTargetsMD add-md ${CURRENT_DIR}/verifyDataBase/ir_and_elf/px4_hitl_pi64.ll --incall-config ${CURRENT_DIR}/configs/px4InCalls.yml --dma-config ${CURRENT_DIR}/configs/px4DMATypeAdd.yml
    fi
}

specify_property() {
    echo "Specify the property to be verified..."
    # Check if we have at least one positional argument
    if [ ${#positional_args[@]} -lt 1 ]; then
        echo "Error: specify_property requires at least 1 positional argument." >&2
        echo "Usage: run_verify.sh -u specify_property <property_name>" >&2
        exit 1
    fi
    
    # Execute the create_IR_config command
    python3 verify-api.py create_IR_config -p "${positional_args[0]}"
}

slice_ir() {
    echo "Generating function level slices from LLVM IR..."
    
    # Execute the slice command
    python3 verify-api.py slicing
}

gen_uppaal_model() {
    echo "Generating UPPAAL model files..."
    
    # Build the command starting with the base
    cmd="python3 verify-api.py gen_uppaal_model"
    
    # # Check if we have positional arguments
    # if [ ${#positional_args[@]} -gt 0 ]; then
    #     # Check if first argument is -o flag
    #     if [ "${positional_args[0]}" = "-o" ]; then
    #         cmd="$cmd -o"
    #         # If there's a directory argument after -o
    #         if [ ${#positional_args[@]} -gt 1 ]; then
    #             cmd="$cmd ${positional_args[1]}"
    #         fi
    #     else
    #         # If first argument is not -o, treat it as directory
    #         cmd="$cmd ${positional_args[0]}"
    #     fi
    # fi
    
    # Execute the command
    eval $cmd
}

instrument() {
    echo "Instrumenting LLVM IR for data collection..."
    
    # Execute the instrument command
    python3 verify-api.py instrument
}

ir_build() {
    echo "Building IR with configuration file..."
    
    # Execute the ir_build command
    python3 verify-api.py ir_build
}

gen_temps() {
    echo "Generating template files using MTGen..."
    
    # Check if we have at least one positional argument
    if [ ${#positional_args[@]} -lt 1 ]; then
        echo "Error: gen_temps requires at least 1 positional argument." >&2
        echo "Usage: run_verify.sh -u gen_temps <ArduPilot|px4>" >&2
        exit 1
    fi
    # Validate the first argument
    case "${positional_args[0]}" in
        "ardupilot"|"px4")
            # Valid first argument
            ;;
        *)
            echo "Error: First argument must be one of: ArduPilot, px4" >&2
            echo "Usage: run_verify.sh -u gen_temps <ArduPilot|px4>" >&2
            exit 1
            ;;
    esac

    # Execute the tempsGen command
    python3 verify-api.py tempsGen -t "${positional_args[0]}"
}

run_sim() {
    echo "Running data collector..."
    
    # Execute the run_sim command
    python3 verify-api.py run_sim
}

create_input_seeds() {
    echo "Creating model input seed file from binary format data..."
    
    # Execute the create_input_seeds command
    python3 verify-api.py create_input_seeds
}

instrument_klee_entry() {
    echo "Instrumenting KLEE entry function..."
    python3 verify-api.py instrument_klee_entry
}

klee() {
    echo "Performing symbolic execution with KLEE..."
    python3 verify-api.py klee --max-memory=120000 --max-time=3600s
}

create_inputs() {
    echo "Converting .ktest files to model input .mi files..."
    python3 verify-api.py create_inputs
}

vectorize() {
    echo "Vectorizing flight control code..."
    # Check if we have at least one positional argument
    if [ ${#positional_args[@]} -lt 1 ]; then
        echo "Error: vectorize requires at least 1 positional argument." >&2
        echo "Usage: run_verify.sh -u vectorize <ArduCopter|PX4>" >&2
        exit 1
    fi
    # Validate the first argument
    case "${positional_args[0]}" in
        "ArduCopter"|"PX4")
            # Valid first argument
            ;;
        *)
            echo "Error: First argument must be one of: ArduCopter, PX4" >&2
            echo "Usage: run_verify.sh -u vectorize <ArduCopter|PX4>" >&2
            exit 1
            ;;
    esac
    
    # Execute the vectorize command
    python3 verify-api.py vectorize -t "${positional_args[0]}"
}

find_slice_criterion() {
    echo "Finding slice criterion using LLM and RAG system..."
    python3 verify-api.py find_slice_criterion
}

upload_pi() {
    echo "Uploading flight control software to Raspberry Pi..."
    
    # Execute the upload_pi command (flight control type is determined from ir_config.flightControl)
    python3 verify-api.py upload_pi
}

download_pi() {
    echo "Downloading dumped data from Raspberry Pi..."
    
    # Execute the download_pi command (flight control type is determined from ir_config.flightControl)
    python3 verify-api.py download_pi
}

run_uav() {
    echo "Running UAV simulation..."
    
    # Build the command starting with the base (flight control type is determined from ir_config.flightControl)
    cmd="python3 verify-api.py run_uav"
    
    # Check for --isTime flag in positional arguments
    for arg in "${positional_args[@]}"; do
        if [ "$arg" = "--isTime" ]; then
            cmd="$cmd --isTime"
            echo "Time instrumentation enabled"
            break
        fi
    done
    
    # Execute the command
    eval $cmd
}

verify() {
    echo "Executing UPPAAL model verification..."
    
    # Build the command starting with the base
    cmd="python3 verify-api.py verify"
    
    # Check for --threads flag in positional arguments
    i=0
    while [ $i -lt ${#positional_args[@]} ]; do
        case "${positional_args[$i]}" in
            "--threads")
                if [ $((i+1)) -lt ${#positional_args[@]} ]; then
                    cmd="$cmd --threads ${positional_args[$((i+1))]}"
                    echo "Using ${positional_args[$((i+1))]} threads for parallel verification"
                    i=$((i+1))  # Skip the next argument as it's the thread count
                else
                    echo "Error: --threads flag requires a number" >&2
                    exit 1
                fi
                ;;
        esac
        i=$((i+1))
    done
    
    # Execute the command
    eval $cmd
}

# --- Main Script Logic ---

run() {
    # Parse the arguments passed to the script
    parse_args "$@"

    # Execute different branches based on the script purpose
    case "$script_purpose" in
        "gen_ir")
            gen_ir
            ;;
        "build_uavs")
            build_uavs
            ;;
        "specify_property")
            specify_property
            ;;
        "slice")
            slice_ir
            ;;
        "gen_uppaal_model")
            gen_uppaal_model
            ;;
        "instrument")
            instrument
            ;;
        "ir_build")
            ir_build
            ;;
        "gen_temps")
            gen_temps
            ;;
        "run_sim")
            run_sim
            ;;
        "create_input_seeds")
        create_input_seeds
        ;;
    "instrument_klee_entry")
        instrument_klee_entry
        ;;
    "klee")
        klee
        ;;
    "create_inputs")
        create_inputs
        ;;
    "vectorize")
        vectorize
        ;;
    "find_slice_criterion")
        find_slice_criterion
        ;;
    "upload_pi")
        upload_pi
        ;;
    "download_pi")
        download_pi
        ;;
    "run_uav")
        run_uav
        ;;
    "verify")
        verify
        ;;
    *)
        # This case should ideally not be reached if purpose validation is enabled in parse_args
        echo "Error: Unknown script purpose '$script_purpose'." >&2
        exit 1
        ;;
    esac
}

# Start the script execution
run "$@";

exit 0