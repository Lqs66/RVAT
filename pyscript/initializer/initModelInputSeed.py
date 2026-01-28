import struct
from ..pyTools.utils import *
from ..pyTools.parseConfig import *
from .pygev.gev import EVA
import pandas as pd
import time
import numpy as np
from collections import defaultdict
import typing
import click
import os
from .modelInputs_pb2 import *

ir_config = parse_IR_config()
_model_gv_template = DTMC + f"/verifyDataBase/inputTemplates/{ir_config.flightControl.lower()}_modelInGV.tmp"
_model_hv_template = DTMC + f"/verifyDataBase/inputTemplates/{ir_config.flightControl.lower()}_modelInHeap.tmp"
# _retval_db = DTMC + "/verifyDataBase/inputTemplates/funcSum.db"
_addr_to_offset = {}

def type_spec_to_string(type_spec):
    """Convert TypeSpec enum to string representation"""
    type_map = {
        TypeSpec.UNKNOWN: 'unknown',
        TypeSpec.INT: 'int',
        TypeSpec.FLOAT: 'float',
        TypeSpec.DOUBLE: 'double',
        TypeSpec.PTR: 'ptr',
        TypeSpec.AGGR: 'aggr'
    }
    return type_map.get(type_spec, 'unknown')

def fillGVsFromBin(gv_binFile: str, modelInputs: ModelInputs):
    '''
    fill the global variables xml node with the data from the binary file

    Parameters:
        gv_binFile: str, path to the binary file of global variables
        modelInputs: ModelInputs, the modelInputs protobuf object

    Returns:
        Void
    '''    

    with open(gv_binFile, 'rb') as bin_file:
        for global_var in modelInputs.globalVars:
            if global_var.type != TypeSpec.AGGR:
                offset = global_var.offset
                bin_file.seek(offset)
                if global_var.type == TypeSpec.INT:
                    size = global_var.size
                    if size == 1:
                        global_var.i64_value = struct.unpack("b", bin_file.read(size))[0]
                    elif size == 2:
                        global_var.i64_value = struct.unpack("h", bin_file.read(size))[0]
                    elif size == 4:
                        global_var.i64_value = struct.unpack("i", bin_file.read(size))[0]
                    elif size == 8:
                        global_var.i64_value = struct.unpack("q", bin_file.read(size))[0]
                    else:
                        ERROR(f"Unsupported type: {type_spec_to_string(global_var.type)}")
                        ERROR(f"Unsupported size: {size}")
                        exit(1)
                elif global_var.type == TypeSpec.FLOAT:
                    global_var.f_value = struct.unpack("f", bin_file.read(global_var.size))[0]
                elif global_var.type == TypeSpec.DOUBLE:
                    global_var.d_value = struct.unpack("d", bin_file.read(global_var.size))[0]
                elif global_var.type == TypeSpec.PTR:
                    # we consider pointer as signed int64, we should be careful use it.
                    # we consider pointer as signed int64, we should be careful use it.
                    # we consider pointer as signed int64, we should be careful use it.
                    # we consider pointer as signed int64, we should be careful use it.
                    # we consider pointer as signed int64, we should be careful use it.
                    global_var.i64_value = struct.unpack("q", bin_file.read(global_var.size))[0]
                else:
                    ERROR(f"Unknown type: {type_spec_to_string(global_var.type)}")
                    exit(1)
            else:
                if not global_var.HasField('members'):
                    ERROR("aggr global variable has no members")
                    exit(1)

                for member in global_var.members.members:
                    base_offset = global_var.offset
                    member_offset = member.member_offset
                    bin_file.seek(base_offset + member_offset)
                    if member.type == TypeSpec.INT:
                        size = member.size
                        if size == 1:
                            member.i64_value = struct.unpack("b", bin_file.read(size))[0]
                        elif size == 2:
                            member.i64_value = struct.unpack("h", bin_file.read(size))[0]
                        elif size == 4:
                            member.i64_value = struct.unpack("i", bin_file.read(size))[0]
                        elif size == 8:
                            member.i64_value = struct.unpack("q", bin_file.read(size))[0]
                        else:
                            ERROR(f"Unsupported type: {type_spec_to_string(member.type)}")
                            ERROR(f"Unsupported size: {size}")
                            exit(1)
                    elif member.type == TypeSpec.FLOAT:
                        member.f_value = struct.unpack("f", bin_file.read(member.size))[0]
                    elif member.type == TypeSpec.DOUBLE:
                        member.d_value = struct.unpack("d", bin_file.read(member.size))[0]
                    elif member.type == TypeSpec.PTR:
                        member.i64_value = struct.unpack("q", bin_file.read(member.size))[0]
                    else:
                        ERROR(f"Unknown type: {type_spec_to_string(member.type)}")
                        exit(1)
                    

def parseHeapBinaryFile(hv_binFile: str):
    """
    parse the heap variable binary file and return a list of heap variable info and a dictionary mapping addresses to actual data.
    
    Parameters:
        hv_binFile: str, path to the binary file containing heap variable data.
    
    Returns:
        tuple: (heap_info_list, heap_data_dict)
            - heap_info_list: list of (hash_val, addr, size)
            - heap_data_dict: addr ——> heap data {addr: context_data}
    """
    heap_info_list = []
    heap_data_dict = {}
    
    with open(hv_binFile, 'rb') as bin_file:
        while True:
            # Read hash value (64 bits)
            hash_bytes = bin_file.read(8)
            if not hash_bytes or len(hash_bytes) < 8:
                break
                
            # Read address (64 bits)
            addr_bytes = bin_file.read(8)
            if not addr_bytes or len(addr_bytes) < 8:
                break
                
            # Read size (64 bits)
            size_bytes = bin_file.read(8)
            if not size_bytes or len(size_bytes) < 8:
                break
            
            hash_val = struct.unpack("<Q", hash_bytes)[0]
            addr = struct.unpack("<Q", addr_bytes)[0]
            size = struct.unpack("<Q", size_bytes)[0]

            # read heap variable data
            context_data = bin_file.read(size)
            # Only break if we couldn't read enough data (size > 0 but got less data)
            if len(context_data) < size:
                break
            
            heap_info_list.append((hash_val, addr, size))
            heap_data_dict[addr] = context_data
    
    return heap_info_list, heap_data_dict

def fillHeapVarsWithData(heap_data_dict: dict, heapVars):
    '''
    fill the heap variables with the data from the heap data dictionary

    Parameters:
        heap_data_dict: dict, addr ——> heap data {addr: context_data}
        heapVars: heap variables

    Returns:
        Void
    '''    
    pointer_to_offset = [] # Collect all ptr type elements

    for heapVar in heapVars:
        addr = heapVar.addr
        
        if addr in heap_data_dict:
            context_data = heap_data_dict[addr]
            size = len(context_data)
            
            members = heapVar.members.members
            
            if members:
                # Handle heap variables with members (aggregate types)
                base_offset = heapVar.offset
                
                # fill the member values
                for member in members:
                    relative_offset = member.member_offset
                    member_size = member.size
                    member_type = member.type
                    
                    if relative_offset + member_size <= size:
                        member_data = context_data[relative_offset:relative_offset + member_size]
                        
                        if member_type == TypeSpec.PTR:
                            member.i64_value = struct.unpack("q", member_data)[0]
                            pointer_to_offset.append(member)
                        elif member_type == TypeSpec.INT:
                            if member_size == 1:
                                member.i64_value = struct.unpack("b", member_data)[0]
                            elif member_size == 2:
                                member.i64_value = struct.unpack("h", member_data)[0]
                            elif member_size == 4:
                                member.i64_value = struct.unpack("i", member_data)[0]
                            elif member_size == 8:
                                member.i64_value = struct.unpack("q", member_data)[0]
                            else:
                                ERROR(f"Unsupported type: {type_spec_to_string(member_type)}")
                                exit(1)
                        elif member_type == TypeSpec.FLOAT:
                            member.f_value = struct.unpack("f", member_data)[0]
                        elif member_type == TypeSpec.DOUBLE:
                            member.d_value = struct.unpack("d", member_data)[0]
                        else:
                            WARNING(f"Unsupported type: {type_spec_to_string(member_type)}")
            else:
                # Handle non-aggregate heap variables
                heap_type = heapVar.type
                heap_size = heapVar.size
                
                # Make sure the size of the heap variable is less than or equal to the size of the context data
                if heap_size <= size:
                    heap_data = context_data[:heap_size]
                    
                    if heap_type == TypeSpec.PTR:
                        heapVar.i64_value = struct.unpack("q", heap_data)[0]
                        pointer_to_offset.append(heapVar)
                    elif heap_type == TypeSpec.INT:
                        if heap_size == 1:
                            heapVar.i64_value = struct.unpack("b", heap_data)[0]
                        elif heap_size == 2:
                            heapVar.i64_value = struct.unpack("h", heap_data)[0]
                        elif heap_size == 4:
                            heapVar.i64_value = struct.unpack("i", heap_data)[0]
                        elif heap_size == 8:
                            heapVar.i64_value = struct.unpack("q", heap_data)[0]
                        else:
                            ERROR(f"Unsupported type: {type_spec_to_string(heap_type)}")
                            exit(1)
                    elif heap_type == TypeSpec.FLOAT:
                        heapVar.f_value = struct.unpack("f", heap_data)[0]
                    elif heap_type == TypeSpec.DOUBLE:
                        heapVar.d_value = struct.unpack("d", heap_data)[0]
                    else:
                        WARNING(f"Unsupported: {type_spec_to_string(heap_type)}")
                else:
                    WARNING(f"Heap variable size {heap_size} exceeds context data size {size}")

    # if pointer_to_offset:
    #     unknownPointers = set()
        
    #     for ele in pointer_to_offset:
    #         if "value" in ele.attrib and ele.attrib["value"] != "":
    #             value = int(ele.attrib["value"], 16)
                
    #             # Check if the pointer value is within the address scope
    #             if value != 0 and addrToOffsetMap.get(value) is None:
    #                 unknownPointers.add(ele)
    #                 # ele.attrib["value"] = "-1"
    #             else:
    #                 # Convert address to offset in xml
    #                 ele.attrib["value"] = str(addrToOffsetMap[value])
        
        # if unknownPointers:
        #     INFO(f"Unkown Pointer size: {len(unknownPointers)}")

def mapPointersToOffsets(model_input: ModelInputs, entry_args_list: list):
    """
    map the pointer value to the offset in the model input bin file
    """
    all_ptr_types = []
    # Collect _addr_to_offset for globalVars
    global_vars = model_input.globalVars
    if global_vars is not None:
        for var in global_vars:
            if var.type == TypeSpec.AGGR:
                base_addr = var.addr
                for member in var.members.members:
                    member_addr = base_addr + member.member_offset
                    member_goff = member.member_offset + var.offset
                    _addr_to_offset[member_addr] = member_goff
                    if member.type == TypeSpec.PTR:
                        all_ptr_types.append(member)
            else:
                addr = var.addr
                offset = var.offset
                _addr_to_offset[addr] = offset
                if var.type == TypeSpec.PTR:
                    all_ptr_types.append(var)

   # Collect _addr_to_offset for heapVars
    heap_vars = model_input.heapVars
    if heap_vars is not None:
        for heapVar in heap_vars:
            if heapVar.type == TypeSpec.AGGR:
                base_addr = heapVar.addr
                for member in heapVar.members.members:
                    member_addr = base_addr + member.member_offset
                    member_goff = member.member_offset + heapVar.offset
                    _addr_to_offset[member_addr] = member_goff
                    if member.type == TypeSpec.PTR:
                        all_ptr_types.append(member)
            else:
                addr = heapVar.addr
                offset = heapVar.offset
                _addr_to_offset[addr] = offset
                if heapVar.type == TypeSpec.PTR:
                    all_ptr_types.append(heapVar)

    # Traverse all ptr nodes and convert value to offset
    for ptr_node in all_ptr_types:
        if ptr_node.HasField("i64_value"):
            value = ptr_node.i64_value
            ptr_addr = value if value >= 0 else (1 << 64) + value # convert signed to unsigned
            if ptr_addr == 0:
                ptr_node.i64_value = -1 # null pointer
            elif ptr_addr in _addr_to_offset:
                ptr_node.i64_value = _addr_to_offset[ptr_addr]
            # else:
            #     ptr_node.i64_value = -1 # unknown pointer
        else:
            ptr_node.i64_value = -1 # null pointer

    # Print entry args 
    # format: id: xx, type: xx, value: xx
    if entry_args_list:
        INFO("Entry Args:")
        for id, type, size, value_bytes in entry_args_list:
            if type.startswith("i"):
                if size == 1:
                    value = struct.unpack("b", value_bytes)[0]
                elif size == 2:
                    value = struct.unpack("h", value_bytes)[0]
                elif size == 4:
                    value = struct.unpack("i", value_bytes)[0]
                elif size == 8:
                    value = struct.unpack("q", value_bytes)[0]
                else:
                    ERROR(f"Unsupported int size: {size}")
                    continue
            elif type == "float":
                if size == 4:
                    value = struct.unpack("f", value_bytes)[0]
                else:
                    ERROR(f"Unsupported float size: {size}")
                    continue
            elif type == "double":
                if size == 8:
                    value = struct.unpack("d", value_bytes)[0]
                else:
                    ERROR(f"Unsupported double size: {size}")
                    continue
            elif type == "ptr":
                if size == 8:
                    addr = struct.unpack("Q", value_bytes)[0]
                    if addr in _addr_to_offset:
                        value = _addr_to_offset[addr]
                        raw_value = addr
                    else:
                        value = f"-1 raw: {addr}" # unknown pointer
                else:
                    ERROR(f"Unsupported ptr size: {size}")
                    continue
            elif type == "aggr":
                value = value_bytes.hex()  # represent as hex string
            else:
                ERROR(f"Unsupported entry arg type: {type}")
                continue
            
            INFO(f" id: {id}, type: {type}, size: {size}, value: {value}, raw: {raw_value}")

def parseEntryArgsBinaryFile(entryArgs_file: str):
    """
    Parse the entry args binary file and return a list of entry arguments.
    
    File format: 1 byte id + 3 bytes size + value
    
    Parameters:
        entryArgs_file: str, path to the binary file containing entry arguments data.
    
    Returns:
        list: list of tuples (id, type, size, value_bytes)
    """
    entry_args_list = []
    params_info = ir_config.params
    with open(entryArgs_file, 'rb') as bin_file:
        while True:
            # Read the 4-byte header (id + size combined)
            header_bytes = bin_file.read(4)
            if not header_bytes or len(header_bytes) < 4:
                break
            
            # Parse the header as little-endian uint32
            # Note: LLVM IR will store as native endian, assuming little-endian here
            header = struct.unpack("<I", header_bytes)[0]
            
            # Extract id (bits 31-24) and size (bits 23-0)
            id_val = (header >> 24) & 0xFF
            size_val = header & 0x00FFFFFF
            
            # Read value data
            value_bytes = bin_file.read(size_val)
            if len(value_bytes) < size_val:
                ERROR(f"Incomplete data: expected {size_val} bytes, got {len(value_bytes)}")
                break
            
            # Get type from params_info based on id
            param_type = "unknown"
            if id_val < len(params_info):
                param_type = params_info[id_val].type
            else:
                WARNING(f"Entry arg id {id_val} not found in params_info")
            
            entry_args_list.append((id_val, param_type, size_val, value_bytes))
    
    return entry_args_list

def createModelInputPB(gv_binFile: str, hv_binFile: str, entryArgs_file: str):
    # check template files
    if not os.path.exists(_model_gv_template):
        ERROR(f"Global variables template file {_model_gv_template} does not exist.")
        exit(1)
    if not os.path.exists(_model_hv_template):
        ERROR(f"Heap variables template file {_model_hv_template} does not exist.")
        exit(1) 
    # check binary files
    if not os.path.exists(gv_binFile):
        ERROR(f"Global variables binary file {gv_binFile} does not exist.")
        exit(1)
    if not os.path.exists(hv_binFile):
        ERROR(f"Heap variables binary file {hv_binFile} does not exist.")
        exit(1)
    if not os.path.exists(entryArgs_file):
        ERROR(f"EntryArgs file {entryArgs_file} does not exist.")
        exit(1)

    gv_timestamp = extractTimestamp(gv_binFile)
    hv_timestamp = extractTimestamp(hv_binFile)
    entry_args_timestamp = extractTimestamp(entryArgs_file)

    if gv_timestamp is None or hv_timestamp is None or entry_args_timestamp is None or gv_timestamp != hv_timestamp or gv_timestamp != entry_args_timestamp:
        ERROR(f"Timestamp mismatch: {gv_timestamp} != {hv_timestamp} != {entry_args_timestamp}")
        exit(1)

    output_filename = f"{parse_IR_config().property}_seed_{gv_timestamp}.mi"
    output_file = os.path.join(DTMC, "verifyDataBase", "model_inputs", parse_IR_config().property, output_filename)
    output_dir = os.path.dirname(output_file)
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        INFO(f"Created directory {output_dir} for model input seed files.")

    modelInputs = ModelInputs()
    gv_template = ModelInputs()
    INFO("Starting to handle global variables...")
    with open(_model_gv_template, "rb") as f:
        gv_template.ParseFromString(f.read())

    # Copy all global variables from template to modelInputs
    for gv in gv_template.globalVars:
        new_gv = modelInputs.globalVars.add()
        new_gv.CopyFrom(gv)
    
    fillGVsFromBin(gv_binFile, modelInputs)
    
    # calculate the maximum offset from global variables to determine starting offset for heap variables
    max_gv_offset = 0
    last_global_var = modelInputs.globalVars[-1]
    if last_global_var.type == TypeSpec.AGGR:
        last_member = last_global_var.members.members[-1]
        max_gv_offset = last_global_var.offset + last_member.member_offset + last_member.size
    else:
        max_gv_offset = last_global_var.offset + last_global_var.size
       
    ''' 
    handle heap variables 
    ''' 
    INFO("Starting to handle heap variables...")
    hv_template = ModelInputs()
    with open(_model_hv_template, "rb") as f:
        hv_template.ParseFromString(f.read())
    
    if hv_template is not None:
        # create hash to node mapping from template
        template_hash_map = {}
        for hv in hv_template.heapVars:
            template_hash_map[hv.hash] = hv

        # parse heap binary file
        heap_info_list, heap_data_dict = parseHeapBinaryFile(hv_binFile)
            
        current_heap_offset = max_gv_offset  # start heap variable offsets after global variables
        
        # print(f"Max global variable offset: {current_heap_offset}")
        # p = False
        # create heap_vars for heap variables
        for hash_val, addr, actual_size in heap_info_list:
            if hash_val in template_hash_map:
                # print(hash_val, addr, actual_size)
                hv = template_hash_map[hash_val]
                template_size = hv.size
                
                # check if the actual size is a multiple of the template size
                if actual_size % template_size == 0:
                    array_count = actual_size // template_size

                    # create the corresponding number of nodes
                    for i in range(array_count):
                        new_hv = modelInputs.heapVars.add()
                        new_hv.CopyFrom(hv)

                        # update address (address of each element)
                        element_addr = addr + i * template_size
                        new_hv.addr = element_addr

                else:
                    WARNING(f"Size mismatch for hash {hash_val}: actual size {actual_size} is not a multiple of template size {template_size}. Treating as bytes array.")
                    new_hv = modelInputs.heapVars.add()
                    new_hv.hash = hash_val
                    new_hv.addr = addr
                    new_hv.size = actual_size
                    new_hv.type = TypeSpec.AGGR
                    for i in range(actual_size):
                        member = new_hv.members.members.add()
                        member.member_offset = i
                        member.size = 1
                        member.type = TypeSpec.INT
                    
            else:
                WARNING(f"No template found for hash value {hash_val}")
                WARNING(f"Skipping heap variable with addr {hex(addr)} and size {actual_size}")
        
        # set offsets for heap variables
        for hv in modelInputs.heapVars:
            hv.offset = current_heap_offset
            current_heap_offset += hv.size
            # if hv.type == TypeSpec.AGGR:
            #     # set member offsets relative to the base offset
            #     for member in hv.members.members:
            #         base_offset = hv.offset
            #         member_offset = member.member_offset
            #         member.offset = base_offset + member_offset

        fillHeapVarsWithData(heap_data_dict, modelInputs.heapVars)
        
        # parse entry args as (id, type, size, value)
        entry_args_list = parseEntryArgsBinaryFile(entryArgs_file)
        mapPointersToOffsets(modelInputs, entry_args_list)
        
        total_bin_heap_size = 0
        for _, _, size in heap_info_list:
            total_bin_heap_size += size

        INFO(f"Total heap variables size: {total_bin_heap_size} bytes")

    # output
    with open(output_file, "wb") as f:
        f.write(modelInputs.SerializeToString())
    
    INFO(f"Model input seed file generated at {output_file}")

import sqlite3
import time
from ..pyTools.funcSumDataBase import insert_return_value, insert_property
def insertRvlToDB(rvl_file: str, property: str):
    '''
    // retValsBinFile: the path of the bin file
    // The format of this file is:
    // The binary file starts with a 4-byte integer, where the high bit is the is_direct flag (0 or 1), the second bit is the is_ptr flag (0 or 1), and the low 30 bits are the id. Followed by 4 bytes of size, and then size bytes of return value.
    // For example: [4-byte val: is_direct|is_ptr|id] [4-byte size] [size bytes data]
    '''
    if not rvl_file:
        ERROR(f"RVL file {rvl_file} does not exist.")
        return

    start_time = time.time()
    conn = sqlite3.connect(_retval_db)
    cursor = conn.cursor()
    cursor.execute("BEGIN TRANSACTION")
    offset = 0
    with open(rvl_file, "rb") as f:
        while True:
            f.seek(offset)
            chunk = f.read(8)
            if not chunk or len(chunk) < 8:
                break
            concat = struct.unpack('<Q', chunk)[0]  # Read as little-endian uint64
            size = concat & 0xFFFFFFFF
            val = concat >> 32
            is_direct = bool(val >> 31)
            is_ptr = bool((val >> 30) & 1)
            id = val & 0x3FFFFFFF
            
            offset += 8
            
            data = f.read(size)
            if not data or len(data) < size:
                break
            offset += size
            # print(f"id: {id}, is_direct: {is_direct}, is_ptr: {is_ptr}, size: {size}")
            if is_ptr:
                if size != 8:
                    ERROR(f"Unexpected size {size} for pointer return value")
                    return
                else:
                    addr = struct.unpack('Q', data)[0]
                    if addr in _addr_to_offset:
                        offset_val = _addr_to_offset[addr]
                        data = struct.pack('Q', offset_val)
                    else:
                        # WARNING(f"No offset found for address {hex(addr)}")
                        data = struct.pack('Q', 0xFFFFFFFFFFFFFFFF)  # Represents -1 in two's complement
            
            pid = insert_property(conn, property, id, is_direct)
            if pid is not None:
                rid = insert_return_value(conn, pid, data)
                if rid is None:
                    WARNING(f"Failed to insert return value for property {property} with pid {pid}")
            # else:
            #     WARNING(f"Property {property} with id {id} and is_direct {is_direct} can't insert into database")
    conn.commit()
    conn.close()
    INFO(f"Insert RVL file {rvl_file} into database {_retval_db} cost {time.time() - start_time} seconds")


@click.command(name="create_input_seeds")
def create_input_seeds():
    """
    Traverse binary files in runtime_data directory, find Global and Heap file pairs with the same timestamp,
    and automatically call createModelInputXML function to generate corresponding XML files.
    """
    property = ir_config.property
    binary_dir = os.path.join(DTMC, "verifyDataBase", "model_inputs", "runtime_data", property)
    
    if not os.path.exists(binary_dir):
        ERROR(f"Runtime data directory {binary_dir} does not exist.")
        exit(1)
    
    INFO(f"Scanning directory: {binary_dir}")
    
    # Find all files with the property prefix
    gv_file = [os.path.join(binary_dir, f) for f in os.listdir(binary_dir) if f.startswith(property + "_Global_") and f.endswith('.in')]
    hv_file = [os.path.join(binary_dir, f) for f in os.listdir(binary_dir) if f.startswith(property + "_Heap_") and f.endswith('.in')]
    entryArgs_file = [os.path.join(binary_dir, f) for f in os.listdir(binary_dir) if f.startswith(property + "_EntryArgs_") and f.endswith('.in')]
    if len(gv_file) != 1 or len(hv_file) != 1 or len(entryArgs_file) != 1:
        ERROR(f"Global, Heap or EntryArgs file not unique for property {property}")
        exit(1)
    # rvl_file = [os.path.join(binary_dir, f) for f in os.listdir(binary_dir) if f.startswith(property + "_Rvl_") and f.endswith('.in')]
    # if len(rvl_file) != 1:
    #     ERROR(f"RVL file not unique for property {property}")
    #     exit(1) 
    INFO(f"Found files:\n Global: {gv_file[0]}\n Heap: {hv_file[0]}\n EntryArgs: {entryArgs_file[0]}")
    # try:
    #     createModelInputPB(gv_file[0], hv_file[0], entryArgs_file[0])
    # except Exception as e:
    #     ERROR(f"Failed to process {gv_file[0]} and {hv_file[0]}")
    #     exit(1)
    createModelInputPB(gv_file[0], hv_file[0], entryArgs_file[0])
    # print(_addr_to_offset)
    # INFO(f"Start insert ret value map into database{_retval_db}")
    # insertRvlToDB(rvl_file[0], property)


