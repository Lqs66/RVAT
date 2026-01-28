import click, pexpect, sys
from .utils import *
from .parseConfig import *

clang = DTMC + "/dependencies/llvm-16.0.4/build/bin/clang"
clangxx = DTMC + "/dependencies/llvm-16.0.4/build/bin/clang++"
ld = DTMC + "/dependencies/llvm-16.0.4/build/bin/ld.lld"
opt = DTMC + "/dependencies/llvm-16.0.4/build/bin/opt"
llvm_dis = DTMC + "/dependencies/llvm-16.0.4/build/bin/llvm-dis"

c_std_libs_dir = "/usr/aarch64-linux-gnu/lib"
gcc_libs_dir = "/usr/lib/gcc-cross/aarch64-linux-gnu/11"
crt1 = c_std_libs_dir + "/crt1.o"
crti = c_std_libs_dir + "/crti.o"
crtbeginT = gcc_libs_dir + "/crtbeginT.o"
crtend = gcc_libs_dir + "/crtend.o"
crtn = c_std_libs_dir + "/crtn.o"

ardupilot_dir = DTMC + "/flight-control/arducopter-4.4"

arducopter_ld = DTMC + "/configs/arducopter.ld"
px4_ld = DTMC + "/configs/px4.ld"

px4_dir = DTMC + "/flight-control/PX4-1.15.2"

# @click.group()
# def cli():
#     """Flight Control Builder"""
#     pass

@click.command(name="uav_build")
@click.option('-t', '--type', type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), 
              required=True, help="flight controls you want to build: ardupilot, px4")
@click.option('-cn', '--conda-name', type=str, default=None, help="Conda env's name, if used")
@click.option('-nop', '--no-opaque-pointers', is_flag=True, default=False, help="Enable -no-opaque-pointers flag when building")
@click.option('-m', '--min-build', is_flag=True, default=False, help="Enable minimal build for flight control")
def uav_build(type, conda_name, no_opaque_pointers, min_build):
    if DTMC is None:
        click.echo("Please set DTMC environment variable to the root of the project")
        return
    if type == 'ardupilot':
        ardupilot_build(conda_name, no_opaque_pointers, min_build)
    elif type == 'px4':
        px4_build(conda_name, no_opaque_pointers)
        # click.echo("px4 is not supported yet")
    else:
        click.echo(f"{type} is invalid, please choose from ardupilot, px4, paparazzi")
        
def ardupilot_build(condaName, no_opaque_pointers, min_build):
    try:
        if condaName is not None:
            click.echo(f"Building ardupilot in conda env {condaName}")
                 
        child = pexpect.spawn("bash", timeout=None, encoding='utf-8')
        child.logfile = sys.stdout
        
        child.expect("$")
            
        if condaName:
            child.sendline(f"conda activate {condaName}")
            child.expect(f"({condaName})")
            click.echo(f"Conda env {condaName} activated")

        child.sendline(f"cd {ardupilot_dir}")
        
        to_disable_feature = {
            "lua": "--disable-scripting",
            "camera": (
                "--define AP_CAMERA_ENABLED=0 "
                "--define HAL_RUNCAM_ENABLED=0 "
            ),
            "videoTX": "--define AP_VIDEOTX_ENABLED=0",
            "osd": (
                "--define OSD_ENABLED=0 "
                "--define HAL_PLUSCODE_ENABLE=0 "
                "--define OSD_PARAM_ENABLED=0 "
                "--define HAL_OSD_SIDEBAR_ENABLE=0"
            ),
            "MSP": "--define HAL_MSP_ENABLED=0",
            # "AP_Notify": (
            #     "--define AP_NOTIFY_MAVLINK_PLAY_TUNE_SUPPORT_ENABLED=0 "
            #     "--define AP_NOTIFY_MAVLINK_LED_CONTROL_SUPPORT_ENABLED=0 "
            #     "--define AP_NOTIFY_NCP5623_ENABLED=0 "
            #     "--define AP_NOTIFY_PROFILED_ENABLED=0 "
            #     "--define AP_NOTIFY_PROFILED_SPI_ENABLED=0 "
            #     "--define AP_NOTIFY_NEOPIXEL_ENABLED=0 "
            # ),
        }
        nop_flag = "-Xclang -no-opaque-pointers" if no_opaque_pointers else ""
        min_build_args = " ".join(to_disable_feature.values()) if min_build else ""
        build_command = (
            f"CC={clang} CXX={clangxx} CFLAGS='-target aarch64-linux-gnu -O0 -g {nop_flag} -gdwarf-4 -fno-use-cxa-atexit' CXXFLAGS='-target aarch64-linux-gnu -O0 -g {nop_flag} -gdwarf-4 -fno-use-cxa-atexit' "
            f"LDFLAGS='-fuse-ld={ld}' ./waf configure --board=sitl --enable-llvm-ir --toolchain=aarch64-linux-gnu --static {min_build_args} && "
            "./waf copter  || exit 1;"
            "exit"
        )
        
        child.sendline(build_command)
        child.expect(pexpect.EOF)
            
        child.close()

        if child.exitstatus != 0:
            raise Exception("Ardupilot build failed")
        
    except Exception as e:
        print(f"Error occurred: {str(e)}")
        if child and not child.closed:
            child.close()
        raise

def px4_build(condaName, no_opaque_pointers):
    try:
        if condaName is not None:
            click.echo(f"Building px4 in conda env {condaName}")
                 
        child = pexpect.spawn("bash", timeout=None, encoding='utf-8')
        child.logfile = sys.stdout
        
        child.expect("$")
            
        if condaName:
            child.sendline(f"conda activate {condaName}")
            child.expect(f"({condaName})")
            click.echo(f"Conda env {condaName} activated")

        child.sendline(f"cd {px4_dir}")
        nop_flag = "\\ -Xclang\\ -no-opaque-pointers" if no_opaque_pointers else ""
        build_command = ( 
            f'CMAKE_ARGS="-DCMAKE_C_COMPILER={clang} '
            f'-DCMAKE_CXX_COMPILER={clangxx} '
            f'-DCMAKE_C_FLAGS=--target=aarch64-linux-gnu\\ -O0\\ -gdwarf-4\\ -flto\\ -fno-discard-value-names\\ -fno-use-cxa-atexit\\ -fembed-bitcode{nop_flag} '
            f'-DCMAKE_CXX_FLAGS=--target=aarch64-linux-gnu\\ -fno-exceptions\\ -O0\\ -gdwarf-4\\ -flto\\ -fno-discard-value-names\\ -fno-use-cxa-atexit\\ -fembed-bitcode\\ -fwhole-program-vtables{nop_flag} '
            f'-DCMAKE_AR={DTMC}/dependencies/llvm-16.0.4/build/bin/llvm-ar '
            f'-DCMAKE_LINKER={ld} '
            f'-DCMAKE_NM={DTMC}/dependencies/llvm-16.0.4/build/bin/nm '
            f'-DCMAKE_RANLIB={DTMC}/dependencies/llvm-16.0.4/build/bin/llvm-ranlib '
            f'-DCMAKE_EXE_LINKER_FLAGS=-fuse-ld=lld\\ -flto\\ -static '
            f'-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld\\ -flto '
            f'-DCMAKE_MODULE_LINKER_FLAGS=-fuse-ld=lld\\ -flto '
            f'-DBUILD_SHARED_LIBS=OFF" make emlid_navio2 || exit 1;'
            'exit'
        )
        
        child.sendline(build_command)
        child.expect(pexpect.EOF)
            
        child.close()

        if child.exitstatus != 0:
            raise Exception("PX4 build failed")
        
    except Exception as e:
        print(f"Error occurred: {str(e)}")
        if child and not child.closed:
            child.close()
        raise

@click.command(name="uav_ir")
@click.option('-t', '--type', type=click.Choice(['ardupilot', 'px4', 'paparazzi'], case_sensitive=False), 
              required=True, help="flight controls you want to generate IR: ardupilot, px4")
@click.option('-nop', '--no-opaque-pointers', is_flag=True, default=False, 
              help="Enable -no-opaque-pointers flag when generating IR")
def uav_ir(type, no_opaque_pointers):
    if DTMC is None:
        click.echo("Please set DTMC environment variable to the root of the project")
        return
    if type == 'ardupilot':
        gen_ardupilot_ir(no_opaque_pointers)
    elif type == 'px4':
        gen_px4_ir(no_opaque_pointers)
        # click.echo("px4 is not supported yet")
    else:
        click.echo(f"{type} is invalid, please choose from ardupilot, px4")

def gen_ardupilot_ir(no_opaque_pointers=False):
    try:
        child = pexpect.spawn(f"bash", timeout=None, encoding='utf-8')
        child.logfile = sys.stdout

        child.sendline(f"cd {ardupilot_dir}/build/sitl")
        
        PreModelChecking = DTMC + "/dependencies/llvm-16.0.4/build/lib/PreModelChecking.so"

        no_opaque_flag = "-Wl,-plugin-opt=no-opaque-pointers" if no_opaque_pointers else ""
        opt_and_dis_no_opaque_flag = "-opaque-pointers=0" if no_opaque_pointers else ""
        target_ir_name = "arducopter_sitl_pi64_nop.ll" if no_opaque_pointers else "arducopter_sitl_pi64.ll"

        ir_command = (
            f"{clangxx} -target aarch64-linux-gnu {no_opaque_flag} -fno-exceptions -Wl,--gc-sections -fuse-ld=lld -flto -Wl,--plugin-opt=emit-llvm -Wl,--lto-whole-program-visibility ArduCopter/*.o -Llib -lArduCopter_libs -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=wholeprogramdevirt -whole-program-visibility -wholeprogramdevirt-summary-action=export arducopter.bc -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=loweratomic arducopter.bc -o arducopter.bc &&"
            # f"{opt} {opt_and_dis_no_opaque_flag} --passes=lowerintrinsics arducopter.bc -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=mergereturn arducopter.bc -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=adce arducopter.bc -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=globaldce arducopter.bc -o arducopter.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --load-pass-plugin={PreModelChecking} -p pre-model-checking arducopter.bc -o arducopter.bc &&" # run SimplifyCFGPass, GlobalOptPass and other custom passes 
            f"{llvm_dis} {opt_and_dis_no_opaque_flag} arducopter.bc -o {target_ir_name} &&"
            f"mkdir -p {DTMC}/verifyDataBase/ir_and_elf &&"
            f"cp {target_ir_name} {DTMC}/verifyDataBase/ir_and_elf/{target_ir_name} || exit 1;"
            "exit"
        )

        child.sendline(ir_command)

        child.expect(pexpect.EOF)

        child.close()

        if child.exitstatus != 0:
            raise Exception("Generate Ardupilot IR failed")
        
    except Exception as e:
        print(f"Error occurred: {str(e)}")
        if child and not child.closed:
            child.close()
        raise

def gen_px4_ir(no_opaque_pointers=False):
    try:
        child = pexpect.spawn(f"bash", timeout=None, encoding='utf-8')
        child.logfile = sys.stdout
        
        child.sendline(f"cd {px4_dir}/build/emlid_navio2_default")
        PreModelChecking = DTMC + "/dependencies/llvm-16.0.4/build/lib/PreModelChecking.so"
        no_opaque_flag = "-Wl,-plugin-opt=no-opaque-pointers" if no_opaque_pointers else ""
        opt_and_dis_no_opaque_flag = "-opaque-pointers=0" if no_opaque_pointers else ""
        target_ir_name = "px4_hitl_pi64_nop.ll" if no_opaque_pointers else "px4_hitl_pi64.ll"
        ir_command = (
            f"{clangxx} -target aarch64-linux-gnu {no_opaque_flag} -fno-exceptions -Wl,--gc-sections -fuse-ld=lld -flto -Wl,--plugin-opt=emit-llvm -Wl,--lto-whole-program-visibility $(find ./platforms/posix/CMakeFiles/px4.dir/ -type f -name '*.o')  -o px4.bc  $(find ./ -type f -name '*.a') &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=wholeprogramdevirt -whole-program-visibility -wholeprogramdevirt-summary-action=export px4.bc -o px4.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=loweratomic px4.bc -o px4.bc &&"
            # f"{opt} {opt_and_dis_no_opaque_flag} --passes=lowerintrinsics px4.bc -o px4.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=mergereturn px4.bc -o px4.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=adce px4.bc -o px4.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --passes=globaldce px4.bc -o px4.bc &&"
            f"{opt} {opt_and_dis_no_opaque_flag} --load-pass-plugin={PreModelChecking} -p pre-model-checking px4.bc -o px4.bc &&" # run SimplifyCFGPass, GlobalOptPass and other custom passes 
            f"{llvm_dis} {opt_and_dis_no_opaque_flag} px4.bc -o {target_ir_name} &&"
            f"mkdir -p {DTMC}/verifyDataBase/ir_and_elf &&"
            f"cp {target_ir_name} {DTMC}/verifyDataBase/ir_and_elf/{target_ir_name} || exit 1;"
            "exit"
        )
        child.sendline(ir_command)
        child.expect(pexpect.EOF)
        child.close()
        if child.exitstatus != 0:
            raise Exception("Generate PX4 IR failed")
    except Exception as e:
        print(f"Error occurred: {str(e)}")
        if child and not child.closed:
            child.close()
        raise


@click.command(name="ir_build")
@click.option('-ir', type=str, required=True, default=None, help="UAV IR you want to build")
@click.option('-p', '--property', type=str, default=None, help="Property name")
@click.option('-t', '--timer', is_flag=True, default=False, help="Enable timer for the build")
def ir_build(ir, property, timer):
    try:
        child = pexpect.spawn(f"bash", timeout=None, encoding='utf-8')
        child.logfile = sys.stdout

        output = DTMC + "/verifyDataBase/ir_and_elf/" + property
        
        ir_filename = ir.split('/')[-1] if '/' in ir else ir
        
        fc_type = ""
        PX4_FLAG = "0"
        if "arducopter" in ir_filename.lower():
            fc_type = "arducopter"
            ld_script = arducopter_ld
        elif "px4" in ir_filename.lower():
            fc_type = "px4"
            ld_script = px4_ld
            PX4_FLAG = "1"
        else:
            ERROR(f"Unknown UAV type in IR file: {ir_filename}")
            exit(1)

        target_obj = output + f"/{fc_type}_{property}.o"
        
        click.echo(f"Building {fc_type} from IR file: {ir}")

        if timer:
            target_exe = output + f"/{fc_type}_{property}_timer"
            
            time_logger_path = DTMC + "/instrumenter/timeLogger"
            time_command = (
                f"cd {time_logger_path} &&"
                f"make CXX={clangxx} &&"
                "cd - &&"
                f"{clangxx} -target aarch64-linux-gnu -c -o {target_obj} -x ir {ir} &&"
                f"{ld} -T {ld_script} -static -o {target_exe} {crt1} {crti} {crtbeginT} -L{c_std_libs_dir} -L{gcc_libs_dir} {target_obj} {time_logger_path}/timeLogger.o -lstdc++ -lm --start-group -lgcc -lgcc_eh -lc --end-group {crtend} {crtn} &&"
                f"cd {time_logger_path} &&"
                "make clean &&"
                f"rm {target_obj} || exit 1;"
                "exit"
            )
            child.sendline(time_command)
        else:
            target_exe = output + f"/{fc_type}_{property}"
            
            memtrack_path = DTMC + "/instrumenter/heapMem"
            command = (
                f"cd {memtrack_path} &&"
                f"make CXX={clangxx} PX4={PX4_FLAG} &&"
                "cd - &&"
                f"{clangxx} -target aarch64-linux-gnu -c -o {target_obj} -x ir {ir} &&"
                f"{ld} -T {ld_script} -static -o {target_exe} {crt1} {crti} {crtbeginT} -L{c_std_libs_dir} -L{gcc_libs_dir} {target_obj} {memtrack_path}/heaptrack.o -lstdc++ -lm --start-group -lgcc -lgcc_eh -lc --end-group {crtend} {crtn} &&"
                f"cd {memtrack_path} &&"
                "make clean &&"
                f"rm {target_obj} || exit 1;"
                "exit"
            )
            child.sendline(command)

        child.expect(pexpect.EOF)

        child.close()

        if child.exitstatus != 0:
            raise Exception(f"{ir} build failed")
        
    except Exception as e:
        print(f"Error occurred: {str(e)}")
        if child and not child.closed:
            child.close()
        raise

# if __name__ == "__main__":
#     cli.add_command(uav_build)
#     cli.add_command(uav_ir)
#     cli.add_command(ir_build)
#     cli()