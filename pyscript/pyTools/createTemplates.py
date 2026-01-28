import os
import sys
import pexpect
import click
from .utils import *
from .funcSumDataBase import create_database

@click.command(name='tempsGen',
              help="Generate template files using MTGen",
              context_settings={
                  'help_option_names': ['-h', '--help']
              })
@click.option('-t', '--fc-type', type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), 
              required=True, help="flight controls you want to build: ardupilot, px4")
@click.option('--output-dir', '-o', 
              default=DTMC + "/verifyDataBase/inputTemplates/",
              help="Directory for output template files and function summary database")
def tempsGen(fc_type, output_dir):
    """Generate template files using MTGen.
    
    This command converts LLVM IR files to template files by calling the MTGen program and 
    generate function summary database.
    The DTMC environment variable needs to be set.
    """
    if fc_type == 'ardupilot':
        input_file = DTMC + "/verifyDataBase/ir_and_elf/arducopter_sitl_pi64.ll"
    elif fc_type == 'px4':
        input_file = DTMC + "/verifyDataBase/ir_and_elf/px4_hitl_pi64.ll"
    else:
        ERROR("Unsupported flight control type. Please choose either 'ardupilot' or 'px4'.")
        sys.exit(1)

    # Create function summary database
    # create_database(os.path.join(output_dir, 'funcSum.db'))
 
    mtgen_path = DTMC + "/modelTempGen/build/MTGen"
    
    if not os.path.exists(mtgen_path):
        ERROR("MTGen executable not found. Please compile the MTGen program first.")
        sys.exit(1)
    
    if not os.path.exists(input_file):
        ERROR("Input file does not exist. Please provide a valid LLVM IR file.")
        sys.exit(1)
    
    os.makedirs(output_dir, exist_ok=True)
    
    command = f"{mtgen_path} {input_file} -o {output_dir}"
    
    # click.echo(click.style(f"Executing command: {command}", fg='blue'))
    
    try:
        # Use pexpect to execute command with no timeout limit
        child = pexpect.spawn(command, timeout=None)
        # Capture and print all output
        child.logfile = sys.stdout.buffer
        child.expect(pexpect.EOF)
        
        child.close()
            
    except Exception as e:
        ERROR(f"An error occurred while executing MTGen: {e}")
        sys.exit(1)

if __name__ == "__main__":
    tempsGen()