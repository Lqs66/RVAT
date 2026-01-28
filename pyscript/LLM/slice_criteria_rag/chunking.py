import os
import sys
import hashlib
from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from pathlib import Path
from pyscript.pyTools.utils import *

try:
    from langchain.text_splitter import RecursiveCharacterTextSplitter, Language
except ImportError:
    raise ImportError("langchain not available. Please install with: pip install langchain")

@dataclass
class Chunk:
    content: str
    start_line: int
    end_line: int
    file_path: str

    def __hash__(self):
        # Use deterministic hash for consistent results across runs
        content_str = f"{self.start_line}_{self.end_line}_{self.file_path}_{self.content}"
        return int(hashlib.md5(content_str.encode()).hexdigest(), 16)


class LangChainChunker:
    """LangChain-based file chunker using RecursiveCharacterTextSplitter"""
    
    def __init__(self, chunk_size: int = 3000, chunk_overlap: int = 200):
        self.chunk_size = chunk_size
        self.chunk_overlap = chunk_overlap
        # Initialize LangChain text splitter for C++ code
        self.text_splitter = RecursiveCharacterTextSplitter.from_language(
            language=Language.CPP,
            chunk_size=chunk_size,
            chunk_overlap=chunk_overlap
        )
    
    def _estimate_line_numbers(self, text: str, chunk_text: str, chunk_index: int) -> tuple[int, int]:
        """Estimate start and end line numbers for a chunk"""
        lines = text.split('\n')
        chunk_lines = chunk_text.split('\n')
        
        # Find the chunk in the original text
        chunk_start_pos = text.find(chunk_text)
        if chunk_start_pos == -1:
            # Fallback: estimate based on chunk index
            lines_per_chunk = len(lines) // max(1, chunk_index + 1)
            start_line = chunk_index * lines_per_chunk + 1
            end_line = min(start_line + len(chunk_lines) - 1, len(lines))
            return start_line, end_line
        
        # Count lines before the chunk
        text_before_chunk = text[:chunk_start_pos]
        start_line = text_before_chunk.count('\n') + 1
        end_line = start_line + len(chunk_lines) - 1
        
        return start_line, end_line
    
    def chunk_text(self, text: str, file_path: str = "") -> List[Chunk]:
        """Split text into chunks using LangChain's RecursiveCharacterTextSplitter"""
        # Use LangChain to split the text
        chunk_texts = self.text_splitter.split_text(text)
        
        chunks = []
        for i, chunk_text in enumerate(chunk_texts):
            start_line, end_line = self._estimate_line_numbers(text, chunk_text, i)
                       
            chunk = Chunk(
                content=chunk_text,
                start_line=start_line,
                end_line=end_line,
                file_path=file_path
            )
            chunks.append(chunk)
        
        return chunks
    
    def chunk_file(self, file_path: str) -> List[Chunk]:
        """Chunk a single file"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                content = f.read()
            return self.chunk_text(content, file_path)
        except Exception as e:
            print(f"Error reading file {file_path}: {e}")
            return []
    
    def chunk_directory(self, directory_path: str, file_extensions: Optional[List[str]] = None) -> List[Chunk]:
        """Chunk all files in a directory"""
        if file_extensions is None:
            file_extensions = ['.cpp', '.c', '.h', '.hpp', '.cc', '.cxx', '.hh']
        
        all_chunks = []
        directory = Path(directory_path)
        
        for file_path in directory.rglob('*'):
            if file_path.is_file() and file_path.suffix in file_extensions:
                print(f"    Chunking file: {file_path}")
                # Remove DTMC prefix from file_path
                file_path_str = str(file_path)
                if DTMC and file_path_str.startswith(DTMC + "/"):
                    file_path_str = file_path_str[len(DTMC + "/"):]
                chunks = self.chunk_file(file_path_str)
                all_chunks.extend(chunks)
        
        return all_chunks

# if __name__ == "__main__":
#     chunker = LangChainChunker(chunk_size=20000, chunk_overlap=200)
#     chunks = chunker.chunk_file("/home/lqs66/Desktop/modelCheckingFlightControl/uppllvm/base/src/base.cpp")
#     for chunk in chunks:
#         print(chunk.content)
#         print(f"Lines: {chunk.start_line}-{chunk.end_line}")
#         print()
