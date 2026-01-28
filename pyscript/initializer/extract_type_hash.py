#!/usr/bin/env python3
"""
Script to extract IRTypeHash values from heap dump files.

The file format is:
- IRTypeHash (size_t, 8 bytes on 64-bit systems)
- Memory address (void*, 8 bytes on 64-bit systems)  
- Size (size_t, 8 bytes on 64-bit systems)
- Memory content (size bytes)
"""

import struct
import sys
import os
from typing import List, Set

def extract_type_hashes(file_path: str) -> List[tuple]:
    """
    Extract all IRTypeHash values and sizes from the binary heap dump file.
    
    Args:
        file_path: Path to the binary heap dump file
        
    Returns:
        List of tuples (IRTypeHash, size)
    """
    type_hashes = []
    
    if not os.path.exists(file_path):
        print(f"Error: File {file_path} does not exist")
        return type_hashes
    
    try:
        with open(file_path, 'rb') as f:
            entry_count = 0
            while True:
                # Read IRTypeHash (size_t, 8 bytes on 64-bit)
                data = f.read(8)
                if len(data) < 8:
                    break  # End of file
                
                type_hash = struct.unpack('<Q', data)[0]  # Little-endian unsigned long long
                
                # Read memory address (void*, 8 bytes on 64-bit)
                data = f.read(8)
                if len(data) < 8:
                    print(f"Warning: Incomplete entry at position {f.tell()-8}")
                    break
                
                addr = struct.unpack('<Q', data)[0]  # Little-endian unsigned long long
                
                # Read size (size_t, 8 bytes on 64-bit)
                data = f.read(8)
                if len(data) < 8:
                    print(f"Warning: Incomplete entry at position {f.tell()-8}")
                    break
                
                size = struct.unpack('<Q', data)[0]  # Little-endian unsigned long long
                
                type_hashes.append((type_hash, addr, size))
                
                # Skip the memory content (size bytes)
                f.seek(size, 1)  # Seek relative to current position
                
                entry_count += 1
                
        print(f"Successfully processed {entry_count} entries")
        
    except Exception as e:
        print(f"Error reading file {file_path}: {e}")
        
    return type_hashes

def print_statistics(type_hashes: List[tuple]):
    """Print statistics about the type hashes."""
    if not type_hashes:
        print("No type hashes found")
        return
    
    unique_hashes = set([h for h, _, _ in type_hashes])
    
    print(f"\nStatistics:")
    print(f"Total entries: {len(type_hashes)}")
    print(f"Unique type hashes: {len(unique_hashes)}")
    print(f"Hash frequency and sizes:")
    
    # Count frequency of each hash and collect sizes
    hash_info = {}
    for hash_val, addr, size in type_hashes:
        if hash_val not in hash_info:
            hash_info[hash_val] = {'count': 0, 'sizes': set()}
        hash_info[hash_val]['count'] += 1
        hash_info[hash_val]['sizes'].add(size)
    
    # Sort by frequency (descending)
    sorted_hashes = sorted(hash_info.items(), key=lambda x: x[1]['count'], reverse=True)
    
    for hash_val, info in sorted_hashes:
        sizes_str = ', '.join(str(s) for s in sorted(info['sizes']))
        print(f"  0x{hash_val:016x} ({hash_val}): {info['count']} times, sizes: {sizes_str}")

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 extract_type_hash.py <heap_dump_file>")
        print("Example: python3 extract_type_hash.py /path/to/PX4_P17_Heap_1760352986.in")
        sys.exit(1)
    
    file_path = sys.argv[1]
    
    print(f"Processing file: {file_path}")
    type_hashes = extract_type_hashes(file_path)
    
    if type_hashes:
        print(f"\nAll IRTypeHash values with sizes:")
        for i, (hash_val, addr, size) in enumerate(type_hashes):
            print(f"{i+1:3d}: hash=0x{hash_val:016x} ({hash_val}), addr=0x{addr:016x}, size={size}")
        
        print_statistics(type_hashes)
        
        # Save to output file
        output_file = file_path + "_type_hashes.txt"
        try:
            with open(output_file, 'w') as f:
                f.write("IRTypeHash values extracted from: " + file_path + "\n")
                f.write("=" * 80 + "\n")
                for i, (hash_val, addr, size) in enumerate(type_hashes):
                    f.write(f"{i+1:3d}: hash=0x{hash_val:016x} ({hash_val}), addr=0x{addr:016x}, size={size}\n")
                
                f.write("\nStatistics:\n")
                f.write(f"Total entries: {len(type_hashes)}\n")
                f.write(f"Unique type hashes: {len(set([h for h, _, _ in type_hashes]))}\n")
                
                # Write detailed hash information
                hash_info = {}
                for hash_val, addr, size in type_hashes:
                    if hash_val not in hash_info:
                        hash_info[hash_val] = {'count': 0, 'sizes': set()}
                    hash_info[hash_val]['count'] += 1
                    hash_info[hash_val]['sizes'].add(size)
                
                sorted_hashes = sorted(hash_info.items(), key=lambda x: x[1]['count'], reverse=True)
                f.write("\nHash frequency and sizes:\n")
                for hash_val, info in sorted_hashes:
                    sizes_str = ', '.join(str(s) for s in sorted(info['sizes']))
                    f.write(f"  0x{hash_val:016x} ({hash_val}): {info['count']} times, sizes: {sizes_str}\n")
                
            print(f"\nResults saved to: {output_file}")
            
        except Exception as e:
            print(f"Warning: Could not save results to file: {e}")
    
    else:
        print("No type hashes found in the file")

if __name__ == "__main__":
    main()