#!/usr/bin/env python3
"""
YAML Merge Script - Indirect Call Analysis Result Merger

This script merges three YAML files (TypeMDInCalls.yml, SVFInCalls.yml, DeepTypeInCalls.yml)
according to the following rules:

1. Prioritize results from TypeMDInCalls.yml
2. If an inCallID is not defined in TypeMDInCalls.yml, use the union of SVFInCalls.yml and DeepTypeInCalls.yml
3. For merging SVFInCalls.yml and DeepTypeInCalls.yml:
   - If both have the same inCallID, take the union of targets
   - If only one has it, use that result directly
"""

import os
import sys
import yaml
import argparse
from pathlib import Path
from typing import Dict, List, Any, Set


def load_yaml_file(file_path: str) -> Dict[str, Any]:
    """
    Load YAML file and return parsed data
    
    Args:
        file_path (str): YAML file path
        
    Returns:
        Dict[str, Any]: Parsed YAML data
        
    Raises:
        FileNotFoundError: File not found
        yaml.YAMLError: YAML parsing error
    """
    if not os.path.exists(file_path):
        raise FileNotFoundError(f"File not found: {file_path}")
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            data = yaml.safe_load(f)
            return data if data is not None else {}
    except yaml.YAMLError as e:
        raise yaml.YAMLError(f"YAML parsing error {file_path}: {e}")


def extract_indirect_calls(yaml_data: Dict[str, Any]) -> Dict[int, Dict[str, Any]]:
    """
    Extract indirectCalls from YAML data and create index by inCallID
    
    Args:
        yaml_data (Dict[str, Any]): YAML data
        
    Returns:
        Dict[int, Dict[str, Any]]: Dictionary with inCallID as key
    """
    indirect_calls = yaml_data.get('indirectCalls', [])
    indexed_calls = {}
    
    for call in indirect_calls:
        in_call_id = call.get('inCallID')
        if in_call_id is not None:
            indexed_calls[in_call_id] = call
    
    return indexed_calls


def merge_targets(targets1: List[str], targets2: List[str]) -> List[str]:
    """
    Merge two targets lists, remove duplicates while preserving order
    
    Args:
        targets1 (List[str]): First targets list
        targets2 (List[str]): Second targets list
        
    Returns:
        List[str]: Merged deduplicated targets list
    """
    # Use set to remove duplicates but preserve original order
    seen = set()
    merged = []
    
    # Add elements from first list
    for target in targets1:
        if target not in seen:
            seen.add(target)
            merged.append(target)
    
    # Add non-duplicate elements from second list
    for target in targets2:
        if target not in seen:
            seen.add(target)
            merged.append(target)
    
    return merged


def merge_indirect_calls(type_md_calls: Dict[int, Dict[str, Any]], 
                        svf_calls: Dict[int, Dict[str, Any]], 
                        deep_type_calls: Dict[int, Dict[str, Any]]) -> List[Dict[str, Any]]:
    """
    Merge three indirect call datasets according to specified rules
    
    Args:
        type_md_calls (Dict[int, Dict[str, Any]]): TypeMDInCalls data
        svf_calls (Dict[int, Dict[str, Any]]): SVFInCalls data  
        deep_type_calls (Dict[int, Dict[str, Any]]): DeepTypeInCalls data
        
    Returns:
        List[Dict[str, Any]]: Merged indirect calls list
    """
    merged_calls = {}
    
    # Get all possible inCallIDs
    all_call_ids = set(type_md_calls.keys()) | set(svf_calls.keys()) | set(deep_type_calls.keys())
    
    for call_id in sorted(all_call_ids):
        # Rule 1: Prioritize TypeMDInCalls results
        if call_id in type_md_calls:
            merged_calls[call_id] = type_md_calls[call_id].copy()
        else:
            # Rule 2: If TypeMDInCalls doesn't have it, use union of SVFInCalls and DeepTypeInCalls
            svf_call = svf_calls.get(call_id)
            deep_type_call = deep_type_calls.get(call_id)
            
            if svf_call and deep_type_call:
                # Both have it, merge targets
                merged_call = svf_call.copy()
                svf_targets = svf_call.get('targets', [])
                deep_targets = deep_type_call.get('targets', [])
                merged_call['targets'] = merge_targets(svf_targets, deep_targets)
                merged_calls[call_id] = merged_call
            elif svf_call:
                # Only SVF has it
                merged_calls[call_id] = svf_call.copy()
            elif deep_type_call:
                # Only DeepType has it
                merged_calls[call_id] = deep_type_call.copy()
    
    # Convert to list and sort by inCallID
    result = [merged_calls[call_id] for call_id in sorted(merged_calls.keys())]
    return result


def save_merged_yaml(merged_data: List[Dict[str, Any]], output_path: str):
    """
    Save merged data to YAML file
    
    Args:
        merged_data (List[Dict[str, Any]]): Merged data
        output_path (str): Output file path
    """
    output_yaml = {
        'indirectCalls': merged_data
    }
    
    with open(output_path, 'w', encoding='utf-8') as f:
        yaml.dump(output_yaml, f, default_flow_style=False, allow_unicode=True, sort_keys=False)


def find_missing_call_ids(type_md_calls: Dict[int, Dict[str, Any]], 
                         svf_calls: Dict[int, Dict[str, Any]], 
                         deep_type_calls: Dict[int, Dict[str, Any]]) -> List[int]:
    """
    Find inCallIDs that don't exist in any of the three files
    
    Args:
        type_md_calls: TypeMDInCalls data
        svf_calls: SVFInCalls data
        deep_type_calls: DeepTypeInCalls data
        
    Returns:
        List[int]: List of missing inCallIDs
    """
    # Get all existing inCallIDs
    existing_ids = set(type_md_calls.keys()) | set(svf_calls.keys()) | set(deep_type_calls.keys())
    
    if not existing_ids:
        return []
    
    # Find ID range
    min_id = min(existing_ids)
    max_id = max(existing_ids)
    
    # Find missing IDs
    missing_ids = []
    for call_id in range(min_id, max_id + 1):
        if call_id not in existing_ids:
            missing_ids.append(call_id)
    
    return missing_ids


def print_merge_statistics(type_md_calls: Dict[int, Dict[str, Any]], 
                         svf_calls: Dict[int, Dict[str, Any]], 
                         deep_type_calls: Dict[int, Dict[str, Any]], 
                         merged_calls: List[Dict[str, Any]]):
    """
    Print merge statistics
    
    Args:
        type_md_calls: TypeMDInCalls data
        svf_calls: SVFInCalls data
        deep_type_calls: DeepTypeInCalls data
        merged_calls: Merged data
    """
    print("\n=== Merge Statistics ===")
    print(f"TypeMDInCalls.yml: {len(type_md_calls)} records")
    print(f"SVFInCalls.yml: {len(svf_calls)} records")  
    print(f"DeepTypeInCalls.yml: {len(deep_type_calls)} records")
    print(f"Total after merge: {len(merged_calls)} records")
    
    # Statistics by priority
    type_md_used = 0
    svf_deep_merged = 0
    svf_only = 0
    deep_only = 0
    
    for call in merged_calls:
        call_id = call['inCallID']
        if call_id in type_md_calls:
            type_md_used += 1
        elif call_id in svf_calls and call_id in deep_type_calls:
            svf_deep_merged += 1
        elif call_id in svf_calls:
            svf_only += 1
        elif call_id in deep_type_calls:
            deep_only += 1
    
    print(f"\nPriority usage statistics:")
    print(f"  From TypeMD: {type_md_used} records")
    print(f"  SVF+DeepType merged: {svf_deep_merged} records")
    print(f"  SVF only: {svf_only} records")
    print(f"  DeepType only: {deep_only} records")
    
    # Find and display missing inCallIDs
    missing_ids = find_missing_call_ids(type_md_calls, svf_calls, deep_type_calls)
    if missing_ids:
        print(f"\n❌ Missing inCallIDs ({len(missing_ids)} total):")
        # Display missing IDs in lines, max 10 per line
        for i in range(0, len(missing_ids), 10):
            ids_line = missing_ids[i:i+10]
            print(f"  {', '.join(map(str, ids_line))}")
    else:
        print(f"\n✅ No missing inCallIDs")


def save_missing_ids(missing_ids: List[int], output_path: str):
    """
    Save missing inCallIDs to file
    
    Args:
        missing_ids (List[int]): List of missing inCallIDs
        output_path (str): Output file path
    """
    if not missing_ids:
        return
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("# inCallIDs that don't exist in any of the three YAML files\n")
        f.write(f"# Total: {len(missing_ids)} missing IDs\n")
        f.write("# Generated at: " + __import__('datetime').datetime.now().strftime("%Y-%m-%d %H:%M:%S") + "\n\n")
        f.write("missing_inCallIDs:\n")
        for missing_id in missing_ids:
            f.write(f"  - {missing_id}\n")


def main():
    """Main function"""
    parser = argparse.ArgumentParser(description='Merge indirect call YAML files')
    parser.add_argument('--type-md', '-t', 
                       default='/home/lqs66/Desktop/modelCheckingFlightControl/preAnalyzer/build/incalls/TypeMDInCalls.yml',
                       help='TypeMDInCalls.yml file path')
    parser.add_argument('--svf', '-s',
                       default='/home/lqs66/Desktop/modelCheckingFlightControl/preAnalyzer/build/incalls/SVFInCalls.yml', 
                       help='SVFInCalls.yml file path')
    parser.add_argument('--deep-type', '-d',
                       default='/home/lqs66/Desktop/modelCheckingFlightControl/preAnalyzer/build/incalls/DeepTypeInCalls.yml',
                       help='DeepTypeInCalls.yml file path')
    parser.add_argument('--output', '-o',
                       default='/home/lqs66/Desktop/modelCheckingFlightControl/preAnalyzer/build/incalls/MergedInCalls.yml',
                       help='Output file path')
    parser.add_argument('--missing-output', '-m',
                       default='/home/lqs66/Desktop/modelCheckingFlightControl/preAnalyzer/build/incalls/MissingInCallIDs.yml',
                       help='Missing inCallID output file path')
    parser.add_argument('--verbose', '-v', action='store_true',
                       help='Show detailed information')
    
    args = parser.parse_args()
    
    try:
        # Load YAML files
        print("Loading YAML files...")
        type_md_data = load_yaml_file(args.type_md)
        svf_data = load_yaml_file(args.svf)
        deep_type_data = load_yaml_file(args.deep_type)
        
        # Extract indirect call data
        print("Extracting indirect call data...")
        type_md_calls = extract_indirect_calls(type_md_data)
        svf_calls = extract_indirect_calls(svf_data)
        deep_type_calls = extract_indirect_calls(deep_type_data)
        
        # Merge data
        print("Merging data...")
        merged_calls = merge_indirect_calls(type_md_calls, svf_calls, deep_type_calls)
        
        # Find missing inCallIDs
        missing_ids = find_missing_call_ids(type_md_calls, svf_calls, deep_type_calls)
        
        # Save results
        print(f"Saving merged results to: {args.output}")
        save_merged_yaml(merged_calls, args.output)
        
        # Save missing inCallIDs
        if missing_ids:
            print(f"Saving missing inCallIDs to: {args.missing_output}")
            save_missing_ids(missing_ids, args.missing_output)
        
        # Print statistics
        if args.verbose:
            print_merge_statistics(type_md_calls, svf_calls, deep_type_calls, merged_calls)
        
        print(f"\n✅ Merge completed! Results saved to: {args.output}")
        if missing_ids:
            print(f"✅ Missing inCallIDs saved to: {args.missing_output}")
        
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()