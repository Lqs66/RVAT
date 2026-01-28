import os
import time
from typing import List
from .vectorDB import QdrantVectorDB
from .chunking import LangChainChunker
from .LLMGenerator import *

from pyscript.pyTools.utils import *
from pyscript.pyTools.parseConfig import *

def vectorization(dir_paths: List[str], collection_name: str):
    start_time = time.time()
    for dir_path in dir_paths:
        if not os.path.exists(dir_path):
            raise ValueError(f"Directory {dir_path} does not exist")
        if not os.path.isdir(dir_path):
            raise ValueError(f"{dir_path} is not a directory")
    chunker = LangChainChunker(chunk_size=20000, chunk_overlap=200)
    db = QdrantVectorDB(collection_name=collection_name)
    all_chunks = []
    for dir_path in dir_paths:
        INFO(f"Chunking directory: {dir_path}")
        chunks = chunker.chunk_directory(dir_path)
        all_chunks.extend(chunks)
    end_time = time.time()
    INFO(f"Total chunks: {len(all_chunks)}")
    INFO(f"Chunking time cost: {end_time - start_time}")
    start_time = time.time()
    db.add_chunks(all_chunks)
    end_time = time.time()
    INFO(f"Vectorization time cost: {end_time - start_time}")

def query(collection_name: str, query_text: str, top_k: int = 5):
    db = QdrantVectorDB(collection_name=collection_name)
    results = db.query(query_text, top_k=top_k)
    res = []
    for result in results:
        res.append({
            "content": result.payload["content"],
            "file_path": result.payload["file_path"],
            "start_line": result.payload["start_line"],
            "end_line": result.payload["end_line"],
            "score": result.score,
        })
    return res

def find_formula(target_property_name: str, property_set_path: str):
    import json
    with open(property_set_path, "r") as f:
        property_set = json.load(f)
    for property in property_set:
        if property["id"] == target_property_name:
            return property["uppaal_formula"], property["property_specification"]
    ERROR(f"Find property {target_property_name} in {property_set_path} failed")
    return None, None

def find_slice_criterias():
    import json
    target_property_name = parse_IR_config().property
    target_flight_control = parse_IR_config().flightControl

    property_slice_criterion_map_path = DTMC + "/configs/property_slice_criterion.json"
    
    # Step 1: Check if property_slice_criterion.json exists
    if os.path.exists(property_slice_criterion_map_path):
        INFO(f"Found existing cache file: {property_slice_criterion_map_path}")
        try:
            with open(property_slice_criterion_map_path, "r") as f:
                cached_data = json.load(f)
                
            # Step 2: Check if the cached data contains the target property
            if isinstance(cached_data, dict) and target_property_name in cached_data:
                INFO(f"Found cached slice criterion for property: {target_property_name}")
                cached_criteria = cached_data[target_property_name]
                
                # Found cached data, no need to call LLM
                return
            else:
                INFO(f"Property {target_property_name} not found in cache, will generate new slice criterion")
        except (json.JSONDecodeError, IOError) as e:
            WARNING(f"Reading cache file: {e}, will recreate it")
    else:
        INFO(f"Cache file does not exist, will create: {property_slice_criterion_map_path}")
        # Ensure the directory exists
        os.makedirs(os.path.dirname(property_slice_criterion_map_path), exist_ok=True)

    # Step 3: Generate slice criterion using LLM
    INFO(f"Generating slice criterion for property: {target_property_name}")
    
    if target_flight_control == "ArduCopter":
        property_set_path = DTMC + "/docs/_data/arducopter_properties.json"
    elif target_flight_control == "PX4":
        property_set_path = DTMC + "/docs/_data/px4_properties.json"
    else:
        raise ValueError(f"Unknown flight control: {target_flight_control}")

    [formula, specification] = find_formula(target_property_name, property_set_path)
    if formula is None or specification is None:
        raise ValueError(f"Find property {target_property_name} in {property_set_path} failed")

    query_results = query(target_flight_control, specification, 3)
    code_snippet_list = Code_Snippet_List()
    for item in query_results:
        code_snippet_list.add(item["content"], item["start_line"], item["end_line"], item["file_path"])

    LLM = OpenRouterLLM()
    res = LLM.find_slice_criterias(formula=formula, code_snippet_list=code_snippet_list)

    if res is None:
        raise ValueError("Find slice criterias failed")

    # Step 4: Save the generated slice criterion to cache
    # Extract slicing_criteria from the LLM response and ensure it's a list
    slicing_criteria = res.get("slicing_criteria", []) if isinstance(res, dict) else []
    if not isinstance(slicing_criteria, list):
        slicing_criteria = [slicing_criteria] if slicing_criteria else []
    
    # Convert LLM format to cache format and merge by formula_term
    formula_term_map = {}
    for item in slicing_criteria:
        if isinstance(item, dict):
            formula_term = item.get("formula_term", "")
            slicing_criterion = {
                "line": item.get("slicing_criterion", {}).get("program_statement", 0),
                "source_file": item.get("slicing_criterion", {}).get("source_file", ""),
                "variables": item.get("slicing_criterion", {}).get("variables", [])
            }
            
            # Merge by formula_term - create list of slicing_criterion for each formula_term
            if formula_term not in formula_term_map:
                formula_term_map[formula_term] = []
            formula_term_map[formula_term].append(slicing_criterion)
    
    # Convert to final cache format with slicing_criterion as list
    cache_criteria = []
    for formula_term, slicing_criterion_list in formula_term_map.items():
        cache_item = {
            "formula_term": formula_term,
            "slicing_criterion": slicing_criterion_list  # Now this is a list instead of a single object
        }
        cache_criteria.append(cache_item)
    
    # Load existing cache or create new one
    cache_data = {}
    if os.path.exists(property_slice_criterion_map_path):
        try:
            with open(property_slice_criterion_map_path, "r") as f:
                cache_data = json.load(f)
        except (json.JSONDecodeError, IOError):
            cache_data = {}
    
    # Update cache with new data
    cache_data[target_property_name] = cache_criteria
    
    try:
        with open(property_slice_criterion_map_path, "w") as f:
            json.dump(cache_data, f, indent=2, ensure_ascii=False)
        INFO(f"Successfully saved slice criterion to cache: {property_slice_criterion_map_path}")
    except IOError as e:
        WARNING(f"Failed to save cache file: {e}")
    
    
    
    

if __name__ == "__main__":
    # Initialize vector database
    # db = QdrantVectorDB(collection_name="test")
    
    # # Initialize chunker
    # chunker = LangChainChunker(chunk_size=20000, chunk_overlap=200)
    
    # # Add chunks to database
    # db.add_chunks(chunker.chunk_file("/home/lqs66/Desktop/modelCheckingFlightControl/uppllvm/base/src/instroperation.cpp"))
    # db.delete_collection()
    # db.display_vectors_with_payload()
    query("ArduCopter", "If during Acro, ACRO_TRAINER is 1 or 2, then the vehicle generates roll correction values opposite to the roll angle.")