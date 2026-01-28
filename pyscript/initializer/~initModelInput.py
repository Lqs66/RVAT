import struct
from lxml import etree
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

'''
    this script is used to fill the model input xml file with the data from the objdump file
    `property`_Global_`timestamp`.in's format is AABBBBC..., where AA BBBB and C are the global variables binary data.
    `property`_Heap_`timestamp`.in's format is 'hash'_'addr'_'size'_'data'..., where 'hash' is the hash value of the heap variable, 'addr' is the address of the heap variable, 'size' is the size of the heap variable, and 'data' is the data of the heap variable.
'''

_model_gv_template = DTMC + "/verifyDataBase/inputTemplates/modelInGVTemplate.xml"
_model_hv_template = DTMC + "/verifyDataBase/inputTemplates/modelInHeapTemplate.xml"
_addrOffsetMap = {} # used to store the address to offset map, which is used to convert the address to offset in the xml file

def findAllElementsWithValueAttr(varsXMLNode: etree.Element):
    '''
    find all elements with value attribute in the global or heap variables
    '''
    elements_with_value = []
    for global_vars in varsXMLNode:
        def traverse(element):
            if "value" in element.attrib:
                elements_with_value.append(element)
            for child in element:
                traverse(child)
        traverse(global_vars)
    return elements_with_value

def fillGVNodeFromBin(gv_binFile: str, gvXMLNode: etree.Element):
    '''
    fill the global variables xml node with the data from the binary file

    Parameters:
        gv_binFile: str, path to the binary file of global variables
        gvXMLNode: etree.Element, the xml node of global variables

    Returns:
        Void
    '''    
    elements_with_value = findAllElementsWithValueAttr(gvXMLNode)
    
    # Parse global variables
    with open(gv_binFile, 'rb') as bin_file:
        for ele in elements_with_value:
            offset = int(ele.attrib["offset"])
            bin_file.seek(offset)
            if ele.attrib["type"].startswith("i"):
                size = int(ele.attrib["size"])
                if size == 1:
                    ele.attrib["value"] = str(struct.unpack("b", bin_file.read(size))[0])
                elif size == 2:
                    ele.attrib["value"] = str(struct.unpack("h", bin_file.read(size))[0])
                elif size == 4:
                    ele.attrib["value"] = str(struct.unpack("i", bin_file.read(size))[0])
                elif size == 8:
                    ele.attrib["value"] = str(struct.unpack("q", bin_file.read(size))[0])
                else:
                    ERROR(f"Unsupported type: {ele.attrib['type']}")
                    ERROR(f"Unsupported size: {size}")
                    exit(1)
            elif ele.attrib["type"] == "float":
                ele.attrib["value"] = str(struct.unpack("f", bin_file.read(int(ele.attrib["size"])))[0])
            elif ele.attrib["type"] == "double":
                ele.attrib["value"] = str(struct.unpack("d", bin_file.read(int(ele.attrib["size"])))[0])
            elif ele.attrib["type"] == "ptr":
                ele.attrib["value"] = hex(struct.unpack("Q", bin_file.read(int(ele.attrib["size"])))[0])
            else:
                ERROR(f"Unknown type: {ele.attrib['type']}")
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
            if not context_data or len(context_data) < size:
                break
            
            heap_info_list.append((hash_val, addr, size))
            heap_data_dict[addr] = context_data
    
    return heap_info_list, heap_data_dict

def fillHeapVarsXMLWithData(heap_data_dict: dict, heapVarsXMLNode: etree.Element):
    '''
    fill the heap variables XML node with the data from the heap data dictionary

    Parameters:
        heap_data_dict: dict, addr ——> heap data {addr: context_data}
        heapVarsXMLNode: etree.Element, XML node for heap variables

    Returns:
        Void
    '''    
    pointer_to_offset = [] # Collect all ptr type elements

    for heapVar in heapVarsXMLNode.findall("heapVar"):
        addr = int(heapVar.attrib["addr"], 16)
        
        if addr in heap_data_dict:
            context_data = heap_data_dict[addr]
            size = len(context_data)
            
            members = heapVar.findall("member")
            
            if members:
                # Handle heap variables with members (aggregate types)
                offsets = [int(member.attrib["offset"]) for member in members]
                base_offset = min(offsets)
                
                # fill the member values
                for member in members:
                    offset = int(member.attrib["offset"])
                    relative_offset = offset - base_offset  # offset relative to the base offset
                    member_size = int(member.attrib["size"])
                    member_type = member.attrib["type"]
                    
                    if relative_offset + member_size <= size:
                        member_data = context_data[relative_offset:relative_offset + member_size]
                        
                        if member_type == "ptr":
                            value = hex(struct.unpack("Q", member_data)[0])
                            member.attrib["value"] = value
                            pointer_to_offset.append(member)
                        elif member_type == "i8":
                            value = str(struct.unpack("b", member_data)[0])
                            member.attrib["value"] = value
                        elif member_type == "i16":
                            value = str(struct.unpack("h", member_data)[0])
                            member.attrib["value"] = value
                        elif member_type == "i32":
                            value = str(struct.unpack("i", member_data)[0])
                            member.attrib["value"] = value
                        elif member_type == "i64":
                            value = str(struct.unpack("q", member_data)[0])
                            member.attrib["value"] = value
                        elif member_type == "float":
                            value = str(struct.unpack("f", member_data)[0])
                            member.attrib["value"] = value
                        elif member_type == "double":
                            value = str(struct.unpack("d", member_data)[0])
                            member.attrib["value"] = value
                        else:
                            WARNING(f"Unsupported type: {member_type}")
            else:
                # Handle non-aggregate heap variables
                heap_type = heapVar.attrib["type"]
                heap_size = int(heapVar.attrib["size"])
                
                # Make sure the size of the heap variable is less than or equal to the size of the context data
                if heap_size <= size:
                    heap_data = context_data[:heap_size]
                    
                    if heap_type == "ptr":
                        value = hex(struct.unpack("Q", heap_data)[0])
                        heapVar.attrib["value"] = value
                        pointer_to_offset.append(heapVar)
                    elif heap_type == "i8":
                        value = str(struct.unpack("b", heap_data)[0])
                        heapVar.attrib["value"] = value
                    elif heap_type == "i16":
                        value = str(struct.unpack("h", heap_data)[0])
                        heapVar.attrib["value"] = value
                    elif heap_type == "i32":
                        value = str(struct.unpack("i", heap_data)[0])
                        heapVar.attrib["value"] = value
                    elif heap_type == "i64":
                        value = str(struct.unpack("q", heap_data)[0])
                        heapVar.attrib["value"] = value
                    elif heap_type == "float":
                        value = str(struct.unpack("f", heap_data)[0])
                        heapVar.attrib["value"] = value
                    elif heap_type == "double":
                        value = str(struct.unpack("d", heap_data)[0])
                        heapVar.attrib["value"] = value
                    else:
                        WARNING(f"Unsupported: {heap_type}")
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

def mapPointersToOffsets(model_input_xml: etree.Element):
    """
    将 XML 中的 ptr 类型节点的 value 转换为偏移，如果指向的地址在 addr-offset 映射中。
    """
    # 假定xml文件为字节数组，收集其中每个member的 addr 到 offset 映射
    addr_to_offset = {}
    # Collect addr_to_offset for globalVars
    global_vars = model_input_xml.find('globalVars')
    if global_vars is not None:
        for var in global_vars.findall('globalVar'):
            if var.attrib.get('type') == 'aggr':
                base_addr = int(var.attrib.get('addr'), 16)
                for member in var.findall('member'):
                    member_addr = int(member.attrib.get('addr', base_addr + int(member.attrib.get('member_offset'))))
                    member_goff = int(member.attrib.get('offset'))
                    addr_to_offset[member_addr] = member_goff
            else:
                addr = int(var.attrib.get('addr'), 16)
                offset = int(var.attrib.get('offset'))
                addr_to_offset[addr] = offset

   # Collect addr_to_offset for heapVars
    heap_vars = model_input_xml.find('heapVars')
    if heap_vars is not None:
        for heapVar in heap_vars.findall('heapVar'):
            if heapVar.attrib.get('type') == 'aggr':
                base_addr = int(heapVar.attrib.get('addr'), 16)
                for member in heapVar.findall('member'):
                    member_addr = int(member.attrib.get('addr', base_addr + int(member.attrib.get('member_offset'))))
                    member_goff = int(member.attrib.get('offset'))
                    addr_to_offset[member_addr] = member_goff
            else:
                addr = int(heapVar.attrib.get('addr'), 16)
                offset = int(heapVar.attrib.get('offset'))
                addr_to_offset[addr] = offset

    # Traverse all ptr nodes and convert value to offset
    for ptr_node in model_input_xml.findall('.//*[@type="ptr"]'):
        value = ptr_node.attrib.get('value')
        if value:
            ptr_addr = int(value, 16)
            if ptr_addr == 0:
                ptr_node.set('value', '-1') # null pointer
            elif ptr_addr in addr_to_offset:
                ptr_node.set('value', str(addr_to_offset[ptr_addr]))
            # else:
            #     ptr_node.set('value', '-2')

def createModelInputXML(gv_binFile: str, hv_binFile: str):
    '''
    create the model input xml file.
    its name is modelInput_<property_name>_<timestamp>.xml.
    '''
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

    gv_timestamp = extractTimestamp(gv_binFile)
    hv_timestamp = extractTimestamp(hv_binFile)
    if gv_timestamp is None or hv_timestamp is None or gv_timestamp != hv_timestamp:
        ERROR(f"Timestamp mismatch: {gv_timestamp} != {hv_timestamp}")
        exit(1)

    output_filename = f"modelInput_{parse_IR_config().property}_{gv_timestamp}.xml"
    output_file = os.path.join(DTMC, "verifyDataBase", "model_inputs", parse_IR_config().property, output_filename)
    
    model_input_xml = etree.Element("modelInputs")
    
    INFO("Starting to handle global variables...")
    gv_template = etree.parse(_model_gv_template).getroot()
    if gv_template.tag == "globalVars":
        model_input_xml.append(gv_template)
    
    fillGVNodeFromBin(gv_binFile, model_input_xml)
    
    # calculate the maximum offset from global variables to determine starting offset for heap variables
    max_gv_offset = 0
    for global_vars in model_input_xml.findall("globalVars"):
        elements_with_value = findAllElementsWithValueAttr(global_vars)
        for ele in elements_with_value:
            offset = int(ele.attrib["offset"])
            size = int(ele.attrib["size"])
            max_gv_offset = max(max_gv_offset, offset + size)
    
    ''' 
    handle heap variables 
    ''' 
    INFO("Starting to handle heap variables...")
    hv_template = etree.parse(_model_hv_template).getroot()
    if hv_template.tag == "heapVars":
        heap_vars_template = hv_template
    
    if heap_vars_template is not None:
        # create new heap variables node
        heap_vars_node = etree.Element("heapVars")

        # create hash to node mapping from template
        template_hash_map = {}
        for heapVar in heap_vars_template.findall("heapVar"):
            if "hash" in heapVar.attrib:
                hash_val = int(heapVar.attrib["hash"])
                template_hash_map[hash_val] = heapVar

        # parse heap binary file
        heap_info_list, heap_data_dict = parseHeapBinaryFile(hv_binFile)
            
        current_heap_offset = max_gv_offset  # start heap variable offsets after global variables
        
        # print(f"Max global variable offset: {current_heap_offset}")

        # create heap_vars_node for heap variables
        for hash_val, addr, actual_size in heap_info_list:
            if hash_val in template_hash_map:
                template_node = template_hash_map[hash_val]
                template_size = int(template_node.attrib["size"])
                
                # check if the actual size is a multiple of the template size
                if actual_size % template_size == 0:
                    array_count = actual_size // template_size

                    # create the corresponding number of nodes
                    for i in range(array_count):
                        new_heap_var = etree.Element("heapVar")
                        # copy all attributes from the template node
                        for key, value in template_node.attrib.items():
                            new_heap_var.set(key, value)

                        # update address (address of each element)
                        element_addr = addr + i * template_size
                        new_heap_var.set("addr", hex(element_addr))

                        # copy all child elements
                        for child in template_node:
                            new_child = etree.Element(child.tag)
                            for key, value in child.attrib.items():
                                new_child.set(key, value)
                            new_heap_var.append(new_child)
                        
                        heap_vars_node.append(new_heap_var)
                else:
                    WARNING(f"Size mismatch for hash {hash_val}: actual size {actual_size} is not a multiple of template size {template_size}")
            else:
                WARNING(f"No template found for hash value {hash_val}")
                WARNING(f"Skipping heap variable with addr {hex(addr)} and size {actual_size}")
        
        # set offsets for heap variables
        aggr_vars = []  # record aggr type variables to set offsets later
        
        for heap_var in heap_vars_node.findall("heapVar"):
            if "type" in heap_var.attrib:
                heap_var.set("offset", str(current_heap_offset))
                current_heap_offset += int(heap_var.attrib["size"])
                
                if heap_var.attrib["type"] == "aggr":
                    aggr_vars.append(heap_var)
                    # set member offsets relative to the base offset
                    for member in heap_var.findall("member"):
                        if "member_offset" in member.attrib:
                            base_offset = int(heap_var.attrib["offset"])
                            member_offset = int(member.attrib["member_offset"])
                            member.set("offset", str(base_offset + member_offset))
        
        model_input_xml.append(heap_vars_node)

        fillHeapVarsXMLWithData(heap_data_dict, heap_vars_node)
        
        mapPointersToOffsets(model_input_xml)
        
        # delete offset attributes from aggr type variables
        for aggr_var in aggr_vars:
            if "offset" in aggr_var.attrib:
                del aggr_var.attrib["offset"]

        total_bin_heap_size = 0
        for _, _, size in heap_info_list:
            total_bin_heap_size += size

        INFO(f"Total heap variables size: {total_bin_heap_size} bytes")

    # output xml
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    
    def indent(elem, level=0):
        i = "\n" + level*"\t"
        if len(elem):
            if not elem.text or not elem.text.strip():
                elem.text = i + "\t"
            if not elem.tail or not elem.tail.strip():
                elem.tail = i
            
            children = list(elem)
            
            for child in children:
                indent(child, level+1)
    
            if children and (not children[-1].tail or not children[-1].tail.strip()):
                children[-1].tail = i
        else:
            if not elem.tail or not elem.tail.strip():
                elem.tail = i
    
    indent(model_input_xml)
    tree = etree.ElementTree(model_input_xml)
    tree.write(output_file, pretty_print=True)
    
    INFO(f"Model input file generated at {output_file}")

@click.command(name="create_inputs")
def create_inputs():
    """
    Traverse binary files in runtime_data directory, find Global and Heap file pairs with the same timestamp,
    and automatically call createModelInputXML function to generate corresponding XML files.
    """
    binary_dir = os.path.join(DTMC, "verifyDataBase", "model_inputs", "runtime_data")
    
    if not os.path.exists(binary_dir):
        ERROR(f"Binary format directory {binary_dir} does not exist.")
        exit(1)
    
    INFO(f"Scanning directory: {binary_dir}")
    
    # Collect all files and group by timestamp
    timestamp_files = defaultdict(dict)
    
    for filename in os.listdir(binary_dir):
        filepath = os.path.join(binary_dir, filename)
        if not os.path.isfile(filepath):
            continue
            
        # Parse filename format: property_Type_timestamp.in
        parts = filename.split('_')
        if len(parts) < 3 or not filename.endswith('.in'):
            continue
            
        # Extract timestamp
        timestamp = extractTimestamp(filepath)
        if timestamp is None:
            WARNING(f"Could not extract timestamp from file: {filename}")
            continue
            
        # Determine file type - only process Global and Heap files
        if '_Global_' in filename:
            timestamp_files[timestamp]['global'] = filepath
        elif '_Heap_' in filename:
            timestamp_files[timestamp]['heap'] = filepath
        # Ignore other file types (e.g., RetVal, etc.)
    
    # Process each timestamp file pair
    processed_count = 0
    for timestamp, files in timestamp_files.items():
        if 'global' in files and 'heap' in files:
            gv_file = files['global']
            hv_file = files['heap']
            
            INFO("-------------------------------")
            INFO(f"Global file: {os.path.basename(gv_file)}")
            INFO(f"Heap file: {os.path.basename(hv_file)}")
            
            try:
                createModelInputXML(gv_file, hv_file)
                processed_count += 1
            except Exception as e:
                ERROR(f"Failed to process timestamp {timestamp}: {str(e)}")
        else:
            missing = []
            if 'global' not in files:
                missing.append('Global')
            if 'heap' not in files:
                missing.append('Heap')
            WARNING(f"Timestamp {timestamp} is missing {', '.join(missing)} file(s)")
    
    INFO(f"Processing completed. Total processed: {processed_count} file pairs")

# addrToOffsetMap = {}
# def getAddrScopes(xml_root):
#     addrToOffsetMap[0] = -1
#     for global_vars in xml_root.findall("globalVars"):
#         for ele in global_vars.findall("globalVar"): 
#             start_addr = int(ele.attrib["addr"], 16)
            
#             if ele.attrib["type"] != "aggr":
#                 offset = int(ele.attrib["offset"])
#                 addrToOffsetMap[start_addr] = offset
#             else:
#                 for member in ele.findall("member"):
#                     offset = int(member.attrib["offset"])
#                     addrToOffsetMap[start_addr + int(member.attrib["member_offset"])] = offset
#                     # start_addr += int(member.attrib["member_offset"])
#     for heap_vars in xml_root.findall("heapVars"):
#         for ele in heap_vars.findall("heapVar"):
#             start_addr = int(ele.attrib["addr"], 16)
#             if ele.attrib["type"] != "aggr":
#                 offset = int(ele.attrib["offset"])
#                 addrToOffsetMap[start_addr] = offset
#             else:
#                 offsets = [int(member.attrib["offset"]) for member in ele.findall("member")]
#                 base_offset = min(offsets) if offsets else 0
#                 for member in ele.findall("member"):
#                     offset = int(member.attrib["offset"])
#                     addrToOffsetMap[start_addr + offset - base_offset] = offset
                    # start_addr += int(member.attrib["size"])
    
    # for pair in addrToOffsetMap.items():
    #     print(f"addr: {hex(pair[0])}, offset: {pair[1]}")

# def deserializeRetValFromBin (retVars: list[etree.Element], retValsBinFile: str):
#     '''
#     deserialize the return values from the bin file

#     Parameters:
#         retVars: list[etree.Element], [fRetVals, inRetVals] list of xml elements 0: fRetVals, 1: inRetVals
#         retValsBinFile: str, the path of the bin file

#     Returns:
#         metaDF: pd.DataFrame, the meta data of the return values which generated from the xml file
#         valueDF: pd.DataFrame, the value data of the return values which generated from the bin file
#     '''
#     metaData = [] # used to store the data of the return values
#     fidOffsetMap = defaultdict(lambda: [0])
#     if retVars[0] is not None:
#         for fret in retVars[0].findall("./fRetVal"):
#             flag = False
#             id = int(fret.attrib["id"])
#             name = fret.attrib["name"]
#             type = fret.attrib["type"]
#             if type == "aggr":
#                 offsets = []
#                 for child in fret:
#                     offset = int(child.attrib["offset"])
#                     offsets.append(offset)
#                     size = int(child.attrib["size"])
#                     child_type = child.attrib["type"]
#                     metaData.append((flag, id, offset, name, child_type, size))
#                 fidOffsetMap[(flag, id)] = offsets
#             else:
#                 offset = 0
#                 size = int(fret.attrib["size"])
#                 metaData.append((flag, id, offset, name, type, size))
    
#     if retVars[1] is not None:
#         for iret in retVars[1].findall("./inRetVal"):
#             flag = True
#             id = int(iret.attrib["id"])
#             name = ""
#             type = iret.attrib["type"]
#             if type == "aggr":
#                 offsets = []
#                 for child in iret:
#                     offset = int(child.attrib["offset"])
#                     offsets.append(offset)
#                     size = int(child.attrib["size"])
#                     child_type = child.attrib["type"]
#                     metaData.append((flag, id, offset, name, child_type, size))
#                 fidOffsetMap[(flag, id)] = offsets
#             else:
#                 offset = 0
#                 size = int(iret.attrib["size"])
#                 metaData.append((flag, id, offset, name, type, size))

#     metaColumnLabels = ["flag", "id", "offset", "name", "type", "size"]
#     metaDF = pd.DataFrame(metaData, columns=metaColumnLabels)
#     metaDF.set_index(["flag", "id" ,"offset"], inplace=True, drop=True)

#     retVals = []
#     seen = set()
#     offset = 0
#     with open(retValsBinFile, 'rb') as bin_file:
#         while True:
#             bin_file.seek(offset)
#             chunk = bin_file.read(4)
#             if not chunk or len(chunk) < 4: # check if the file is at the end
#                 break
#             val = struct.unpack("i", chunk)[0]
#             flag = bool(val >> 31)
#             id = val & 0x7FFFFFFF

#             offsets = fidOffsetMap[(flag, id)]
#             offset += 4
#             totalSize = 0
#             for off in offsets:
#                 size = metaDF.loc[(flag, id, off), "size"]
#                 totalSize += size
#                 bin_file.seek(offset + off)
#                 chunk = bin_file.read(size)
#                 if not chunk or len(chunk) < size: # check if the file is at the end
#                     break
#                 formatStr = f"{size}B"
#                 val = struct.unpack(formatStr, chunk)
#                 entry = (flag, id, off, val)
#                 if entry not in seen:
#                     seen.add(entry)
#                     retVals.append(entry)
            
#             offset += totalSize
            
#     valueDF = pd.DataFrame(retVals, columns=["flag", "id", "offset", "value"])
#     return metaDF, valueDF

# def estimate_datas_range(datas: typing.List, block_size=30, percent=0.98):
#     '''
#     estimate the range of the return values

#     Parameters:
#         datas: List, the return values
#         block_size: int, the size of the block
#         percent: float, percentile point

#     Returns:
#         min_val: int, the minimum value of the return values
#         max_val: int, the maximum value of the return values
#     '''
#     datas = np.array(datas)
#     datas = datas[np.isfinite(datas)]
#     if len(datas) == 0:
#         print(f"[WARNING] all the values are nan, return 0, 0")
#         return 0, 0
#     min_value = np.min(datas)
#     max_value = np.max(datas)
#     if len(datas) <= block_size:
#         return min_value, max_value
#     model = EVA(data = pd.Series(datas))
#     model.get_extremes("low",block_size)
#     model.fit_model()
#     min_val = model.extremes_transformer.transform(model.model.ppf(percent))
#     model.get_extremes("high",block_size)
#     model.fit_model()
#     max_val = model.extremes_transformer.transform(model.model.ppf(percent))   
#     if abs(min_val - min_value) > min_value * 0.25:
#         min_val = min_value
#     if abs(max_val - max_value) > max_value * 0.25:
#         max_val = max_value
#     return min_val, max_val

# def genRetValsRange(retVars: list[etree.Element], retValsBinFile: str, block_size=30):
#     '''
#     generate the range of the return values

#     Parameters:
#         retVars: list[etree.Element], the return values of the calls
#         retValsBinFile: str, the path of the bin file
#         block_size: int, the size of the block, used for the GEV model to estimate the range of the return values

#     Returns:
#         resultDF: pd.DataFrame, the range of the return values
#     '''
#     metaDF, valueDF = deserializeRetValFromBin(retVars, retValsBinFile)
#     groupValueDF = valueDF.groupby(["flag", "id", "offset"])
#     result = []
#     # we use key to get type of the return value
#     # and rowIdxes to get the value of the return value
#     for key, rowIdxes in groupValueDF.groups.items():
#         flag, id, offset = key
#         if (flag, id, offset) in metaDF.index: # guard
#             type = metaDF.loc[(flag, id, offset), "type"]
#             # If the type is a pointer or boolean, put all values into a set and return the set
#             if type in ["ptr", "i1"]: 
#                 byte_values = valueDF.loc[rowIdxes, "value"].tolist()
#                 if type == "ptr":
#                     vals = set(hex(int.from_bytes(val, byteorder='little', signed=False)) for val in byte_values)
#                     vals = set(addrToOffsetMap.get(int(val, 16), val) for val in vals)
#                 else:
#                     vals = set(int.from_bytes(val, byteorder='little', signed=False) for val in byte_values)
#                 # valsStr = "{" + ", ".join(str(val) for val in vals) + "}"
#                 result.append((flag, id, offset, vals))
#             elif type == "i8":
#                 byte_values = valueDF.loc[rowIdxes, "value"].tolist()
#                 values = [int.from_bytes(val, byteorder='little', signed=True) for val in byte_values]
#                 min_val = min(values)
#                 max_val = max(values)
#                 if min_val == max_val:
#                     result.append((flag, id, offset, {min_val}))
#                 else:
#                     result.append((flag, id, offset, (min_val, max_val)))
#             else: # for other base types, we use GEV to get the range of the return values  
#                 # Some function ret value can't fit gev, such as _ZNK12AP_Scheduler5ticksEv, gettimeofday, _ZN6AP_HAL6microsEv, _ZN6AP_HAL8micros64Ev .etc. 
#                 byte_values = valueDF.loc[rowIdxes, "value"].tolist()
#                 if flag == False and id in ([519, 17201] + list(range(17210, 17220))):
#                     if type in ["i16", "i32", "i64"]:
#                         byte_values = valueDF.loc[rowIdxes, "value"].tolist()
#                         values = [int.from_bytes(val, byteorder='little', signed=True) for val in byte_values]
#                         ret_val = max(values) + 1
#                         # print(f"[WARNING] {id} {type} {ret_val}")
#                         result.append((flag, id, offset, {ret_val})) #TODO: here we should predict the next time value
#                     continue

#                 if type != "float" and type != "double":
#                     values = [int.from_bytes(val, byteorder='little', signed=True) for val in byte_values]
#                     min_val, max_val = estimate_datas_range(values, block_size)
#                     min_val = int(min_val)
#                     max_val = int(max_val)
#                     # print(f"min: {min}, max: {max}")
#                     if min_val == max_val:
#                         result.append((flag, id, offset, {min_val}))
#                     else:
#                         result.append((flag, id, offset, (min_val, max_val)))
#                 elif type == "float":
#                     # little_bytes_values = [byte_tuple[::-1] for byte_tuple in byte_values] # little endian
#                     values = [struct.unpack("f", struct.pack("I", int.from_bytes(byte_tuple, byteorder='little')))[0] for byte_tuple in byte_values]
#                     min_val, max_val = estimate_datas_range(values, block_size)
#                     if min_val == max_val:
#                         result.append((flag, id, offset, {min_val}))
#                     else:
#                         result.append((flag, id, offset, (min_val, max_val)))
#                 elif type == "double":
#                     # little_bytes_values = [byte_tuple[::-1] for byte_tuple in byte_values]
#                     values = [struct.unpack("d", struct.pack("Q", int.from_bytes(byte_tuple, byteorder='little')))[0] for byte_tuple in byte_values]
#                     min_val, max_val = estimate_datas_range(values, block_size)
#                     if min_val == max_val:
#                         result.append((flag, id, offset, {min_val}))
#                     else:
#                         result.append((flag, id, offset, (min_val, max_val)))

#     tempDF = pd.DataFrame(result, columns=["flag", "id", "offset", "value"])
#     tempDF.set_index(["flag", "id", "offset"], inplace=True, drop=True)

#     resultDF = metaDF.loc[tempDF.index].copy()
#     resultDF["value"] = tempDF["value"]
#     # resultDF.to_csv("result.csv")
#     return resultDF     

# def convertRetValDataFrameToXMLNodes(resultDF: pd.DataFrame):
#     '''
#     convert the return value data frame to xml nodes

#     Parameters:
#         resultDF: pd.DataFrame, the return value data frame

#     Returns:
#         fRetVals: etree.Element, the return values of the functions
#         inRetVals: etree.Element, the return values of the indircet calls
#     '''
#     def convertValueRange(vals):
#         childs = []
#         if isinstance(vals, set):
#             for value in vals:
#                 child= etree.Element("element", attrib={"value": str(value)})
#                 childs.append(child)
#         elif isinstance(vals, tuple):
#             if len(vals) != 2:
#                 ERROR(f"invalid vals size: {vals}")
#                 exit(1)
#             for value in vals:
#                 lower = etree.Element("lower", attrib={"value": str(vals[0])})
#                 upper = etree.Element("upper", attrib={"value": str(vals[1])})
#                 childs = [lower, upper]
#         return childs

#     fRetVals = etree.Element("fRetVals")
#     inRetVals = etree.Element("inRetVals")

#     for (flag, id), group in resultDF.groupby(["flag", "id"]):
#         if len(group) > 1:
#             tag = "inRetVal" if flag else "fRetVal"
#             retVal = etree.Element(tag)
#             retVal.set("id", str(id))
#             if group.iloc[0]["name"] != "":
#                 retVal.set("name", group.iloc[0]["name"])
#             retVal.set("type", "aggr")
#             retVal.set("size", str(group["size"].sum()))
#             for _, row in group.iterrows():
#                 child = etree.Element("member")
#                 child.set("offset", str(row.name[2])) # row.name[2] is the offset because the index is a tuple of (flag, id, offset)
#                 child.set("type", row["type"])
#                 child.set("size", str(row["size"]))
#                 # child.set("value_range", str(row["value"]))
#                 vals = convertValueRange(row["value"])
#                 for v in vals:
#                     child.append(v)
#                 retVal.append(child)
#             if flag:
#                 inRetVals.append(retVal)
#             else:
#                 fRetVals.append(retVal)
#         else:
#             row = group.iloc[0]
#             tag = "inRetVal" if flag else "fRetVal"
#             retVal = etree.Element(tag)
#             retVal.set("id", str(id))
#             if row["name"] != "":
#                 retVal.set("name", row["name"])
#             retVal.set("type", row["type"])
#             retVal.set("size", str(row["size"]))
#             # retVal.set("value_range", str(row["value"]))
#             vals = convertValueRange(row["value"])
#             for v in vals:
#                 retVal.append(v)
#             if flag:
#                 inRetVals.append(retVal)
#             else:
#                 fRetVals.append(retVal)
    
#     return fRetVals, inRetVals

