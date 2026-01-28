from elftools.elf.elffile import ELFFile
import sys

def parse_elf_sections(filename):
    """
    使用pyelftools库解析ELF文件，提取每个section的地址范围。
    假设为ARM ELF，输出十进制地址。
    """
    try:
        with open(filename, 'rb') as f:
            elf = ELFFile(f)
            
            # 检查是否为ARM ELF (e_machine = 40 for ARM)
            if elf.header['e_machine'] != 'ARM':
                print("警告: 这可能不是ARM ELF文件 (e_machine != ARM)。")
            
            print(f"ELF文件: {filename}")
            print("Section 名称\t起始地址 (dec)\t结束地址 (dec)")
            print("-" * 50)
            
            # 遍历所有sections
            for section in elf.iter_sections():
                name = section.name
                addr = section['sh_addr']
                size = section['sh_size']
                
                if addr != 0 and size > 0:  # 只考虑有地址和大小的section
                    start_addr = addr
                    end_addr = addr + size
                    print(f"{name}\t{start_addr}\t{end_addr}")
                    
    except Exception as e:
        print(f"错误: 解析失败 - {e}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("用法: python elf_parser.py <elf_filename>")
        sys.exit(1)
    
    parse_elf_sections(sys.argv[1])