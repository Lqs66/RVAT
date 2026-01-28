import struct
import sys
import os
from . import modelInputs_pb2
from ..pyTools.utils import *
from ..pyTools.parseConfig import *
import click

version_no = 3

class KTestError(Exception):
    pass

class KTest:
    @staticmethod
    def fromfile(path):
        try:
            f = open(path, 'rb')
        except IOError:
            click.echo('file %s not found' % path)
            sys.exit(1)

        hdr = f.read(5)
        if len(hdr) != 5 or (hdr != b'KTEST' and hdr != b'BOUT\n'):
            raise KTestError('unrecognized file')
        version, = struct.unpack('>i', f.read(4))
        if version > version_no:
            raise KTestError('unrecognized version')
        numArgs, = struct.unpack('>i', f.read(4))
        args = []
        for i in range(numArgs):
            size, = struct.unpack('>i', f.read(4))
            args.append(str(f.read(size).decode(encoding='ascii')))

        if version >= 2:
            symArgvs, = struct.unpack('>i', f.read(4))
            symArgvLen, = struct.unpack('>i', f.read(4))
        else:
            symArgvs = 0
            symArgvLen = 0

        numObjects, = struct.unpack('>i', f.read(4))
        objects = []
        for i in range(numObjects):
            size, = struct.unpack('>i', f.read(4))
            name = f.read(size).decode('utf-8')
            size, = struct.unpack('>i', f.read(4))
            bytes = f.read(size)
            objects.append((name, bytes))

        # Create an instance
        b = KTest(version, path, args, symArgvs, symArgvLen, objects)
        return b

    def __init__(self, version, path, args, symArgvs, symArgvLen, objects):
        self.version = version
        self.path = path
        self.symArgvs = symArgvs
        self.symArgvLen = symArgvLen
        self.args = args
        self.objects = objects

class KTestObject:
    def __init__(self, global_name, start_offset, end_offset, bytes_data):
        self.global_name = global_name
        self.start_offset = start_offset
        self.end_offset = end_offset
        self.bytes = bytes_data

# ArduPilot properties that require heap processing
ARDUPILOT_HEAP_PROPERTIES = ['A_ALT_HOLD1', 'A_GPS_FS2']

# Function: Parse input pb, replace values, and generate new .mi
# ArduPilot symbolic copter global variable and heap.
def replace_pb_values_ardupilot(obj_list, model_inputs, testname, input_dir):
    for item in obj_list:
        global_name = item.global_name
        start_offset = item.start_offset
        end_offset = item.end_offset
        blob = item.bytes
        data_size = len(blob)

        # Check if this is a heap variable and if current property requires heap processing
        current_property = parse_IR_config().property
        if global_name == "heap" and current_property in ARDUPILOT_HEAP_PROPERTIES:
            # Process heap similar to PX4
            heap_base_offset = None
            if model_inputs.heapVars:
                heap_base_offset = min(int(hv.offset) for hv in model_inputs.heapVars)
            
            if heap_base_offset is None:
                WARNING(f"No heap variables found in heapVars for {global_name}")
                continue
            
            # Collect all heap members organized by offset
            heap_items = []
            for hv in model_inputs.heapVars:
                hv_offset = int(hv.offset)
                
                if hv.HasField('members'):
                    # Process heapVars with members (aggregate types)
                    for member in hv.members.members:
                        absolute_offset = (hv_offset - heap_base_offset) + int(member.member_offset)
                        heap_item = {
                            "absolute_offset": absolute_offset,
                            "type": member.type,
                            "size": int(member.size),
                            "element": member
                        }
                        heap_items.append(heap_item)
                else:
                    # Process basic type heapVars
                    absolute_offset = hv_offset - heap_base_offset
                    heap_item = {
                        "absolute_offset": absolute_offset,
                        "type": hv.type,
                        "size": int(hv.size),
                        "element": hv
                    }
                    heap_items.append(heap_item)
            
            if not heap_items:
                WARNING(f"No heap members found in heapVars for {global_name}")
                continue
            
            # Process heap items
            found_start = False
            for heap_item in heap_items:
                absolute_offset = heap_item['absolute_offset']
                
                if not found_start:
                    if absolute_offset == start_offset:
                        found_start = True
                    else:
                        continue
                
                if absolute_offset >= end_offset:
                    break
                
                relative_offset = absolute_offset - start_offset
                if relative_offset < 0 or relative_offset + heap_item['size'] > data_size:
                    ERROR(f"Error: Heap member at offset {absolute_offset} out of bounds for {global_name}")
                    sys.exit(1)
                
                data_slice = blob[relative_offset : relative_offset + heap_item['size']]
                
                if heap_item['type'] == modelInputs_pb2.TypeSpec.PTR:
                    new_value = struct.unpack('<Q', data_slice)[0]
                    heap_item['element'].i64_value = new_value
                elif heap_item['type'] == modelInputs_pb2.TypeSpec.INT:
                    if heap_item['size'] == 1:
                        fmt = '<b'
                    elif heap_item['size'] == 2:
                        fmt = '<h'
                    elif heap_item['size'] == 4:
                        fmt = '<i'
                    elif heap_item['size'] == 8:
                        fmt = '<q'
                    else:
                        WARNING(f"Unsupported int size {heap_item['size']} for heap member")
                        continue
                    new_value = struct.unpack(fmt, data_slice)[0]
                    heap_item['element'].i64_value = new_value
                elif heap_item['type'] == modelInputs_pb2.TypeSpec.FLOAT:
                    new_value = struct.unpack('<f', data_slice)[0]
                    heap_item['element'].f_value = new_value
                elif heap_item['type'] == modelInputs_pb2.TypeSpec.DOUBLE:
                    new_value = struct.unpack('<d', data_slice)[0]
                    heap_item['element'].d_value = new_value
                else:
                    WARNING(f"Unsupported type {heap_item['type']} for heap member")
                    continue
            
            if not found_start:
                WARNING(f"No heap member found with offset exactly equal to start_offset {start_offset} for {global_name}")
            continue
        
        # Process regular global variables (not heap)
        members = []
        for gv in model_inputs.globalVars:
            if gv.name != global_name:
                continue
            
            if gv.HasField('members'):
                for member in gv.members.members:
                    mem = {
                        "member_offset": int(member.member_offset),
                        "type": member.type,
                        "size": int(member.size),
                        "element": member
                    }
                    members.append(mem)
        
        if members:
            found_start = False
            for mem in members:
                mem_offset = mem['member_offset']
                if not found_start:
                    if mem_offset == start_offset:
                        found_start = True
                    else:
                        continue
                
                if mem_offset >= end_offset:
                    break
                relative_offset = mem_offset - start_offset
                if relative_offset < 0 or relative_offset + mem['size'] > data_size:
                    ERROR(f"Error: Member at offset {mem_offset} out of bounds for {global_name}")
                    sys.exit(1)
                data_slice = blob[relative_offset : relative_offset + mem['size']]
                if mem['type'] == modelInputs_pb2.TypeSpec.PTR:
                    new_value = struct.unpack('<Q', data_slice)[0]
                    mem['element'].i64_value = new_value
                elif mem['type'] == modelInputs_pb2.TypeSpec.INT:
                    if mem['size'] == 1:
                        fmt = '<b'
                    elif mem['size'] == 2:
                        fmt = '<h'
                    elif mem['size'] == 4:
                        fmt = '<i'
                    elif mem['size'] == 8:
                        fmt = '<q'
                    else:
                        WARNING(f"Unsupported int size {mem['size']} for {global_name}")
                        continue
                    new_value = struct.unpack(fmt, data_slice)[0]
                    mem['element'].i64_value = new_value
                elif mem['type'] == modelInputs_pb2.TypeSpec.FLOAT:
                    new_value = struct.unpack('<f', data_slice)[0]
                    mem['element'].f_value = new_value
                elif mem['type'] == modelInputs_pb2.TypeSpec.DOUBLE:
                    new_value = struct.unpack('<d', data_slice)[0]
                    mem['element'].d_value = new_value
                else:
                    WARNING(f"Unsupported type {mem['type']} for {global_name}")
                    continue

                # print(f"  Replaced member at offset {mem_offset} with value {new_value} for {global_name}")
            
            if not found_start:
                WARNING(f"No member found with member_offset exactly equal to start_offset {start_offset} for {global_name}")
                return
        else:
            # basic type
            WARNING(f"GlobalVar {global_name} is basic type, not member type")
    
    # create new .mi file
    if testname.startswith('ktest'):
        test_number = testname[5:]
        output_filename = os.path.join(input_dir, f"{parse_IR_config().property}_ktest{test_number}.mi")
    else:
        output_filename = os.path.join(input_dir, f"{parse_IR_config().property}_{testname}.mi")
        
    with open(output_filename, 'wb') as f:
        f.write(model_inputs.SerializeToString())
    # Use INFO instead of print to avoid I/O blocking in multithreaded environment
    # INFO(f"Generated updated model input file: {output_filename}")


# PX4 only symbolic heap.
def replace_pb_values_px4(obj_list, model_inputs, testname, input_dir):
    # Find the base offset of the first heapVar in the pb
    # This is the starting offset of heap in the global variable
    heap_base_offset = None
    if model_inputs.heapVars:
        heap_base_offset = min(int(hv.offset) for hv in model_inputs.heapVars)
    
    if heap_base_offset is None:
        WARNING(f"No heap variables found in heapVars")
        return
    
    # Collect all heap members organized by offset
    heap_items = []
    for hv in model_inputs.heapVars:
        hv_offset = int(hv.offset)
        
        if hv.HasField('members'):
            # Process heapVars with members (aggregate types)
            for member in hv.members.members:
                # Calculate absolute offset in heap: (heapVar offset - base) + member offset
                absolute_offset = (hv_offset - heap_base_offset) + int(member.member_offset)
                item = {
                    "absolute_offset": absolute_offset,
                    "type": member.type,
                    "size": int(member.size),
                    "element": member  # Reference to the protobuf Member object for direct modification
                }
                heap_items.append(item)
        else:
            # Process basic type heapVars
            absolute_offset = hv_offset - heap_base_offset
            item = {
                "absolute_offset": absolute_offset,
                "type": hv.type,
                "size": int(hv.size),
                "element": hv  # Reference to the protobuf HeapVar object for direct modification
            }
            heap_items.append(item)
    
    if not heap_items:
        WARNING(f"No heap members found in heapVars")
        return
    
    # Process each object in obj_list
    for item in obj_list:
        global_name = item.global_name
        start_offset = item.start_offset
        end_offset = item.end_offset
        blob = item.bytes
        data_size = len(blob)

        # print(f"start_offset: {start_offset}, end_offset: {end_offset}, data_size: {data_size}")

        # Find the heap_var in heapVars list
        # All heap variables are organized under the "heap" global variable
        if global_name != "heap":
            WARNING(f"Expected global_name to be 'heap' but got '{global_name}' for PX4")
            continue
        
        if heap_items:
            found_start = False
            for item in heap_items:
                absolute_offset = item['absolute_offset']
                
                if not found_start:
                    if absolute_offset == start_offset:
                        found_start = True
                    else:
                        continue
                
                if absolute_offset >= end_offset:
                    break
                
                relative_offset = absolute_offset - start_offset
                if relative_offset < 0 or relative_offset + item['size'] > data_size:
                    ERROR(f"Error: Heap member at offset {absolute_offset} out of bounds")
                    sys.exit(1)
                
                data_slice = blob[relative_offset : relative_offset + item['size']]
                
                # Replace the value based on type
                if item['type'] == modelInputs_pb2.TypeSpec.PTR:
                    new_value = struct.unpack('<Q', data_slice)[0]
                    item['element'].i64_value = new_value
                elif item['type'] == modelInputs_pb2.TypeSpec.INT:
                    if item['size'] == 1:
                        fmt = '<b'
                    elif item['size'] == 2:
                        fmt = '<h'
                    elif item['size'] == 4:
                        fmt = '<i'
                    elif item['size'] == 8:
                        fmt = '<q'
                    else:
                        WARNING(f"Unsupported int size {item['size']} for heap member")
                        continue
                    new_value = struct.unpack(fmt, data_slice)[0]
                    item['element'].i64_value = new_value
                elif item['type'] == modelInputs_pb2.TypeSpec.FLOAT:
                    new_value = struct.unpack('<f', data_slice)[0]
                    item['element'].f_value = new_value
                elif item['type'] == modelInputs_pb2.TypeSpec.DOUBLE:
                    new_value = struct.unpack('<d', data_slice)[0]
                    item['element'].d_value = new_value
                else:
                    WARNING(f"Unsupported type {item['type']} for heap member")
                    continue
            
            if not found_start:
                WARNING(f"No heap member found with offset exactly equal to start_offset {start_offset}")
                continue
    
    # Create new .mi file
    if testname.startswith('ktest'):
        test_number = testname[5:]
        output_filename = os.path.join(input_dir, f"{parse_IR_config().property}_ktest{test_number}.mi")
    else:
        output_filename = os.path.join(input_dir, f"{parse_IR_config().property}_{testname}.mi")
    
    with open(output_filename, 'wb') as f:
        f.write(model_inputs.SerializeToString())
    # Use INFO instead of print to avoid I/O blocking in multithreaded environment
    # INFO(f"Generated updated model input file: {output_filename}")


def parse_ktest_objects(ktest):
    obj_list = []
    for obj_name, data in ktest.objects:
        parts = obj_name.rsplit('_', 2)
        if len(parts) != 3:
            ERROR(f"Invalid object name format (not name_start_end): {obj_name}")
            continue

        global_name, start_offset_str, end_offset_str = parts

        try:
            start_offset = int(start_offset_str)
            end_offset = int(end_offset_str)
            if start_offset > end_offset:
                WARNING(f"Invalid offsets (start > end) for {obj_name}")
                continue
            if end_offset - start_offset != len(data):
                WARNING(f"Invalid offsets (end - start != data size) for {obj_name}")
                continue
        except ValueError:
            ERROR(f"Invalid offsets in object name {obj_name}")
            sys.exit(1)

        obj_list.append(KTestObject(global_name, start_offset, end_offset, data))
    return obj_list

def modelImputsBuilder(seed, ktests):
    model_inputs = modelInputs_pb2.ModelInputs()
    with open(seed, 'rb') as f:
        model_inputs.ParseFromString(f.read())

    input_dir = os.path.dirname(seed)

    for file in ktests:
        ktest = KTest.fromfile(file)
        base = os.path.basename(file)
        testname = 'k' + base.replace('.ktest', '') if base.endswith('.ktest') else base

        obj_list = parse_ktest_objects(ktest)
        # for item in obj_list:
        #     if item.end_offset - item.start_offset != len(item.bytes):
        #         print(f"Error: Data size {len(item.bytes)} smaller than expected {item.end_offset - item.start_offset} for {item.global_name}")
        #         continue
        #     print(item.global_name, item.start_offset, item.end_offset, len(item.bytes))

        if parse_IR_config().flightControl == 'ArduCopter':
            replace_pb_values_ardupilot(obj_list, model_inputs, testname, input_dir)
        elif parse_IR_config().flightControl == 'PX4':
            replace_pb_values_px4(obj_list, model_inputs, testname, input_dir)
        else:
            ERROR(f"Unsupported flight control system: {parse_IR_config().flightControl}")
            sys.exit(1)

def process_single_ktest(seed_bytes, ktest_path, input_dir, flight_control):
    """Process a single ktest file - designed for multiprocessing"""
    try:
        # Each process creates its own model_inputs from bytes
        model_inputs = modelInputs_pb2.ModelInputs()
        model_inputs.ParseFromString(seed_bytes)
        
        ktest = KTest.fromfile(ktest_path)
        base = os.path.basename(ktest_path)
        testname = 'k' + base.replace('.ktest', '') if base.endswith('.ktest') else base
        
        obj_list = parse_ktest_objects(ktest)
        
        if flight_control == 'ArduCopter':
            replace_pb_values_ardupilot(obj_list, model_inputs, testname, input_dir)
        elif flight_control == 'PX4':
            replace_pb_values_px4(obj_list, model_inputs, testname, input_dir)
        else:
            return (ktest_path, f"Unsupported flight control: {flight_control}")
        
        return (ktest_path, None)
    except Exception as e:
        import traceback
        return (ktest_path, traceback.format_exc())

# if __name__ == '__main__':
#     modelImputsBuilder()