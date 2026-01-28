import sys
from modelInputs_pb2 import *
import xml.etree.ElementTree as ET
from xml.dom import minidom

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

def protobuf_to_xml(model_inputs):
    root = ET.Element('ModelInputs')
    
    for gv in model_inputs.globalVars:
        gv_elem = ET.SubElement(root, 'GlobalVar')
        type_str = type_spec_to_string(gv.type)
        gv_elem.set('offset', str(gv.offset))
        gv_elem.set('name', gv.name)
        gv_elem.set('type', type_str)
        gv_elem.set('size', str(gv.size))
        gv_elem.set('addr', str(gv.addr))
        
        # Handle oneof data field
        if gv.HasField('i64_value'):
            gv_elem.set('value', str(gv.i64_value))
        elif gv.HasField('f_value'):
            gv_elem.set('value', str(gv.f_value))
        elif gv.HasField('d_value'):
            gv_elem.set('value', str(gv.d_value))
        elif gv.HasField('members'):
            for member in gv.members.members:
                mem_elem = ET.SubElement(gv_elem, 'Member')
                mem_elem.set('member_offset', str(member.member_offset))
                mem_type_str = type_spec_to_string(member.type)
                mem_elem.set('type', mem_type_str)
                mem_elem.set('size', str(member.size))
                if member.HasField('i64_value'):
                    mem_elem.set('value', str(member.i64_value))
                elif member.HasField('f_value'):
                    mem_elem.set('value', str(member.f_value))
                elif member.HasField('d_value'):
                    mem_elem.set('value', str(member.d_value))
    
    for hv in model_inputs.heapVars:
        hv_elem = ET.SubElement(root, 'HeapVar')
        hv_elem.set('hash', str(hv.hash))
        type_str = type_spec_to_string(hv.type)
        hv_elem.set('offset', str(hv.offset))
        hv_elem.set('type', type_str)
        hv_elem.set('size', str(hv.size))
        hv_elem.set('addr', str(hv.addr))
        
        # Handle oneof data field
        if hv.HasField('i64_value'):
            hv_elem.set('value', str(hv.i64_value))
        elif hv.HasField('f_value'):
            hv_elem.set('value', str(hv.f_value))
        elif hv.HasField('d_value'):
            hv_elem.set('value', str(hv.d_value))
        elif hv.HasField('members'):
            for member in hv.members.members:
                mem_elem = ET.SubElement(hv_elem, 'Member')
                mem_elem.set('member_offset', str(member.member_offset))
                mem_type_str = type_spec_to_string(member.type)
                mem_elem.set('type', mem_type_str)
                mem_elem.set('size', str(member.size))
                if member.HasField('i64_value'):
                    mem_elem.set('value', str(member.i64_value))
                elif member.HasField('f_value'):
                    mem_elem.set('value', str(member.f_value))
                elif member.HasField('d_value'):
                    mem_elem.set('value', str(member.d_value))
    
    return root

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print('Usage: python deserialize_to_xml.py <input_proto_file> <output_xml_file>')
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    model_inputs = ModelInputs()
    with open(input_file, 'rb') as f:
        model_inputs.ParseFromString(f.read())
    
    xml_root = protobuf_to_xml(model_inputs)
    # Debug: print raw XML to see what's causing the parsing error
    raw_xml = ET.tostring(xml_root, encoding='unicode')
    
    try:
        xml_str = minidom.parseString(raw_xml).toprettyxml(indent='  ')
    except Exception as e:
        print(f"XML parsing error: {e}")
        print(f"Problematic XML around column 112: {raw_xml[100:120]}")
        # Write raw XML for debugging
        with open(output_file, 'w') as f:
            f.write(raw_xml)
        print(f'Raw XML written to {output_file} for debugging')
        sys.exit(1)

    with open(output_file, 'w') as f:
        f.write(xml_str)
    print(f'XML written to {output_file}')