import os
import click
import pandas as pd
from bs4 import BeautifulSoup # pip install beautifulsoup4
from tqdm import tqdm
import json
import numpy as np
import ollama
import time
import hashlib
# import chromadb # pip install chromadb
from qdrant_client import QdrantClient # pip install qdrant_client
from qdrant_client.models import Distance, VectorParams, PointStruct, ScrollRequest
from sklearn.metrics.pairwise import cosine_similarity # pip install scikit-learn
from sklearn.metrics import silhouette_score, calinski_harabasz_score, davies_bouldin_score
from sklearn.cluster import OPTICS
from hdbscan.validity import validity_index
import hdbscan # pip install hdbscan
import matplotlib.pyplot as plt # pip install matplotlib
import seaborn as sns # pip install seaborn

from pyscript.pyTools.utils import *
# import networkx as nx

def html_extract_paragraphs(html_file):
    """Extract paragraphs from an HTML file."""
    try:
        with open(html_file, 'r', encoding='utf-8') as f:
            content = f.read()
        soup = BeautifulSoup(content, 'html5lib')
        # Find all paragraphs
        paragraphs = []
        for p_tag in soup.find_all('p'):
            text = p_tag.get_text().replace('\n', ' ').strip()
            if text:
                paragraphs.append({'text': text})
        return paragraphs
    except Exception as e:
        click.echo(f"handle {html_file} failed: {e}")
        return []

def html_find_files(directory):
    """Find all HTML files in the given directory."""
    html_files = []
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.html') or file.endswith('.htm'):
                html_files.append(os.path.join(root, file))
    return html_files

def ardupilot_doc_extract(path):
    """Extract paragraphs from ArduPilot documentation."""
    if not os.path.isdir(path):
        click.echo(f"Directory is not existed: {path}")
        return
    
    # Find HTML files for ArduPilot
    files = html_find_files(path)
    
    output_dir = os.path.join(os.environ.get('DTMC', ''), 'pyscript', 'LLM', 'property_extraction', 'data', 'ardupilot_doc_pairs')
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    total_ps = 0
    for file in tqdm(files, desc="Handle ArduPilot documents"):
        paragraphs = []
        if file.endswith('.html'):
            paragraphs = html_extract_paragraphs(file)
        
        if paragraphs:
            basename = os.path.basename(file)
            name_without_ext = os.path.splitext(basename)[0]
            output_file = os.path.join(output_dir, f"{name_without_ext}.csv")
            
            df = pd.DataFrame(paragraphs)
            df.drop_duplicates(inplace=True)
            df.reset_index(inplace=True, drop=True)
            df.index.name = 'id'
            df.to_csv(output_file)
            total_ps += len(paragraphs)
    
    if total_ps > 0:
        click.echo(f"Total {total_ps} pairs of paragraphs are extracted")
    else:
        click.echo("No paragraphs are extracted")

def md_find_files(directory):
    """Find all Markdown files in the given directory."""
    md_files = []
    for root, _, files in os.walk(directory):
        for file in files:
            if file.endswith('.md'):
                md_files.append(os.path.join(root, file))
    return md_files

def parse_html_table(content):
    soup = BeautifulSoup(content, 'html.parser')
    rows = []
    
    for tr in soup.find_all('tr'):
        row_text = tr.get_text(separator=' ', strip=True)
        if row_text:
            rows.append(row_text)
    
    return rows

def clean_md_format(content):
    """Clean Markdown format."""
    soup = BeautifulSoup(content, 'html.parser')
    text = soup.get_text(separator=' ', strip=True)
    return text

def px4_doc_extract(path):
    """Extract paragraphs from PX4 documentation."""
    if not os.path.isdir(path):
        click.echo(f"Directory is not existed: {path}")
        return
    
    # Find Markdown files for PX4
    files = md_find_files(path)
    
    output_dir = os.path.join(os.environ.get('DTMC', ''), 'pyscript', 'LLM', 'property_extraction', 'data', 'px4_doc_pairs')
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)

    total_ps = 0
    paragraphs = []
    headers_to_split_on = [
        ("#", "Header 1"),
        ("##", "Header 2"),
        ("###", "Header 3"),
        ("####", "Header 4"),
        ("#####", "Header 5"),
        ("######", "Header 6"),
    ]
    for file in tqdm(files, desc="Handle PX4 documents"):
        if file.endswith('.md'):
            from langchain_text_splitters import MarkdownHeaderTextSplitter, RecursiveCharacterTextSplitter
            markdown_splitter = MarkdownHeaderTextSplitter(headers_to_split_on=headers_to_split_on)
            md_splits = markdown_splitter.split_text(open(file, 'r', encoding='utf-8').read())

            character_splitter = RecursiveCharacterTextSplitter(
                chunk_size=1000,
                chunk_overlap=0,
            )

            cleaned_splits = []
            from langchain_core.documents import Document
            for split in md_splits:
                content = split.page_content
                metadata = split.metadata
                if '<tr>' in content.lower():
                    table_rows = parse_html_table(content)
                    for row in table_rows:
                        if row.strip():
                            cleaned_splits.append(
                                Document(
                                    page_content=row,
                                    metadata=metadata
                                )
                            )
                else:
                    # clear md format
                    content = clean_md_format(content)
                    cleaned_splits.append(
                        Document(
                            page_content=content,
                            metadata=metadata
                        )
                    )

            splits = character_splitter.split_documents(cleaned_splits)
            for split in splits:
                paragraphs.append(split.page_content)
    
    if paragraphs:
        df = pd.DataFrame(paragraphs, columns=['text'])
        df.drop_duplicates(inplace=True)
        df.reset_index(inplace=True, drop=True)
        df.index.name = 'id'
        df.to_csv(os.path.join(output_dir, 'px4.csv'))
        total_ps += len(paragraphs)
    
    if total_ps > 0:
        click.echo(f"Total {total_ps} pairs of paragraphs are extracted")
    else:
        click.echo("No paragraphs are extracted")
        
@click.command(name="uav_doc_extract")
@click.option('-t', '--fc-type', type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), 
              required=True, help="flight controls you want to build: ardupilot, px4")
@click.option('-p', '--path', type=str, required=True, help='FC doc directory')
def uav_doc_extract(fc_type, path):
    # Find target documents
    if fc_type == 'ardupilot':
        ardupilot_doc_extract(path)
    elif fc_type == 'px4':
        px4_doc_extract(path)

##########################################################################################################
##########################################################################################################
##########################################################################################################

def reorganize_csv_indices(input_csv, output_csv=None):
    """
    Read a CSV file with id and text columns, reorganize indices, and save to output.
    
    Args:
        input_csv (str): Path to input CSV file.
        output_csv (str, optional): Path to output CSV file. If None, will overwrite the input file.
    
    Returns:
        bool: True if successful, False otherwise.
    """
    try:
        df = pd.read_csv(input_csv, index_col=0)
        df.drop_duplicates(subset=['text'], inplace=True)
        df.reset_index(inplace=True, drop=True)
        df.index.name = 'id'
        output_path = output_csv if output_csv else input_csv
        df.to_csv(output_path)
        click.echo(f"Successfully reorganized indices in {output_path}")
        return True    
    except Exception as e:
        click.echo(f"Failed to reorganize CSV indices: {str(e)}")
        return False

@click.command(name='reorganize_csv')
@click.option('-i', '--input', required=True, type=str, help='Path to input CSV file')
@click.option('-o', '--output', type=str, help='Path to output CSV file')
def reorganize_csv(input, output):
    reorganize_csv_indices(input, output)

def remove_illegal_items(json_obj):
    """
    Remove items that have 'No' or 'N/A' as property_specification and remove entries
    whose properties become empty after filtering.
    
    Args:
        json_obj (list): List of dictionaries containing properties
        
    Returns:
        list: Filtered list with invalid properties removed
    """
    if not json_obj:
        return []
    
    filtered_json = []
    
    for item in json_obj:
        if "properties" not in item or not isinstance(item["properties"], list):
            continue
            
        # Filter out properties with "No" or "N/A" specification
        valid_properties = [
            prop for prop in item["properties"] 
            if prop.get("property_specification") not in ["No", "N/A", "no", "n/a"]
        ]
        
        # Only keep items that have at least one valid property
        if valid_properties:
            item_copy = item.copy()
            item_copy["properties"] = valid_properties
            filtered_json.append(item_copy)
    
    return filtered_json

##########################################################################################################
##########################################################################################################
##########################################################################################################

def get_vectors_from_qdrant(fc_type: str, host: str = "localhost", port: int = 6333):
    """Get vectors and payload data from Qdrant vector database.
    
    Args:
        fc_type: Type of flight control system ("ardupilot" or "px4")
        host: Qdrant server host
        port: Qdrant server port
        
    Returns:
        tuple: (vectors_array, index_to_payload_mapping)
    """
    qdrant_client = QdrantClient(host=host, port=port)
    
    if fc_type == "ardupilot":
        collection_name = "ardupilot_wiki"
    elif fc_type == "px4":
        collection_name = "px4_wiki"
    else:
        raise ValueError("fc_type must be ardupilot or px4")
    
    # Get all points from the collection
    scroll_result = qdrant_client.scroll(
        collection_name=collection_name,
        limit=10000,  # Adjust based on your data size
        with_vectors=True,
        with_payload=True
    )
    
    points = scroll_result[0]
    vectors = []
    index_to_payload_mapping = {}
    
    for idx, point in enumerate(points):
        vectors.append(point.vector)
        index_to_payload_mapping[idx] = {
            'property_specification': point.payload.get('property_specification', ''),
            'source': point.payload.get('source', ''),
            'main_id': point.payload.get('main_id', ''),
            'sub_id': point.payload.get('sub_id', 0),
            'uppaal_formula': point.payload.get('uppaal_formula', ''),
            'flight_control_type': point.payload.get('flight_control_type', '')
        }
    
    return np.array(vectors), index_to_payload_mapping

def HDBSCAN_semantic_cluster(fc_type: str, host: str = "localhost", port: int = 6333, is_print: bool = True):
    """Use HDBSCAN to cluster similar properties.
    
    Args:
        fc_type: Type of flight control system ("ardupilot" or "px4")
        host: Qdrant server host
        port: Qdrant server port
        
    Returns:
        tuple: (cluster_labels, index_to_payload_mapping)
    """
    # Get vectors and payload mapping from Qdrant
    vectors, index_to_payload_mapping = get_vectors_from_qdrant(fc_type, host, port)

    print(f"Retrieved {len(vectors)} vectors from Qdrant")
    print(f"Vector dimension: {vectors.shape[1] if len(vectors) > 0 else 0}")

    clusterer = hdbscan.HDBSCAN(
        min_cluster_size=2,
        min_samples=1,
        cluster_selection_epsilon=0.00,
        metric='cosine',
        algorithm='generic',
        gen_min_span_tree=True  # Enable for DBCV calculation
    )
    clusterer.fit(vectors)
    cluster_labels = clusterer.labels_
    
    # Get DBCV score from HDBSCAN's built-in relative validity
    dbcv_score = clusterer.relative_validity_
    
    # Calculate Silhouette Score (only if we have more than one cluster and some non-noise points)
    silhouette_avg = None
    non_noise_mask = cluster_labels != -1
    non_noise_labels = cluster_labels[non_noise_mask]
    
    if len(set(non_noise_labels)) > 1 and len(non_noise_labels) > 1:
        try:
            silhouette_avg = silhouette_score(vectors[non_noise_mask], non_noise_labels, metric='cosine')
        except Exception as e:
            print(f"Warning: Could not calculate Silhouette score: {e}")
            silhouette_avg = None
    
    # Print clustering results information
    print("\n=== Clustering Results ===")
    print(f"Total points: {len(cluster_labels)}")
    print(f"Number of clusters: {len(set(cluster_labels)) - (1 if -1 in cluster_labels else 0)}")
    print(f"Number of noise points: {np.sum(cluster_labels == -1)}")
    print(f"DBCV Score: {dbcv_score:.4f} (higher is better)")
    if silhouette_avg is not None:
        print(f"Silhouette Score: {silhouette_avg:.4f} (range: -1 to 1, higher is better)")
    else:
        print("Silhouette Score: N/A (requires at least 2 clusters with non-noise points)")

    if is_print:
        # Organize payload data by clusters
        clusters_data = {}
        noise_data = []
        
        # Iterate through all points and group by cluster labels
        for idx, label in enumerate(cluster_labels):
            payload = index_to_payload_mapping.get(idx, f"Unknown_payload_{idx}")
            
            if label == -1:  # Noise points
                noise_data.append({
                    "index": idx,
                    "payload": payload
                })
            else:  # Normal clusters
                cluster_key = f"cluster_{label}"
                if cluster_key not in clusters_data:
                    clusters_data[cluster_key] = []
                clusters_data[cluster_key].append({
                    "index": idx,
                    "payload": payload
                })
        
        # Output results to files
        import json
        
        # Create output directory
        output_dir = os.path.join(DTMC, "pyscript/LLM/property_extraction/data/HDBSCAN")
        os.makedirs(output_dir, exist_ok=True)
        
        # Output clustering results and noise points separately
        clusters_file = os.path.join(output_dir, f"{fc_type}_clusters.json")
        noise_file = os.path.join(output_dir, f"{fc_type}_noise.json")
        
        # Write clustering results file
        with open(clusters_file, 'w', encoding='utf-8') as f:
            json.dump(clusters_data, f, indent=2, ensure_ascii=False)
        
        # Write noise points file
        with open(noise_file, 'w', encoding='utf-8') as f:
            json.dump(noise_data, f, indent=2, ensure_ascii=False)
    
    # Create complete clustering results for return
    clustering_result = {
        "clusters": clusters_data,
        "noise": noise_data,
        "summary": {
            "total_points": len(cluster_labels),
            "num_clusters": len(set(cluster_labels)) - (1 if -1 in cluster_labels else 0),
            "num_noise_points": int(np.sum(cluster_labels == -1)),
            "dbcv_score": float(dbcv_score) if dbcv_score is not None else None,
            "silhouette_score": float(silhouette_avg) if silhouette_avg is not None else None
        }
    }
    
    return clustering_result

def clustering_to_json(fc_type: str, output_file: str = None, host: str = "localhost", port: int = 6333, threshold: float = 0.9):
    """
    Export clustering results to JSON file in the specified format.
    
    Args:
        fc_type: Type of flight control system ("ardupilot" or "px4")
        output_file: Output JSON file path. If None, will use default naming
        host: Qdrant server host
        port: Qdrant server port
        threshold: Similarity threshold for clustering
        
    Returns:
        str: Path to the output JSON file
    """
    # Get clustering results from semantic_textual_similarity
    vectors, result_clusters, cluster_labels, similarity_matrix = semantic_textual_similarity(fc_type, host, port, threshold)
    
    # Prepare output data structure
    output_data = []
    continuous_id = 0  # Counter for continuous cluster IDs
    
    # Process each cluster
    for cluster_idx, cluster_data in enumerate(result_clusters):
        cluster_items = cluster_data['items']
        
        # Skip empty clusters or clusters with only one element
        if len(cluster_items) <= 1:
            continue
            
        # Create cluster entry with continuous ID
        cluster_entry = {
            "id": continuous_id,
            "properties": []
        }
        continuous_id += 1  # Increment for next valid cluster
        
        # Process first item (reference item)
        first_item = cluster_items[0]
        first_payload = first_item['payload']
        
        first_property = {
            "source": first_payload.get('source', ''),
            "property": {
                "id": first_payload.get('main_id', ''),
                "sid": first_payload.get('sub_id', 0),
                "property_specification": first_payload.get('property_specification', ''),
                "uppaal_formula": first_payload.get('uppaal_formula', ''),
                "reason": ""
            }
        }
        cluster_entry["properties"].append(first_property)
        
        # Process remaining items in cluster (calculate similarity to first item)
        for item_idx in range(1, len(cluster_items)):
            item = cluster_items[item_idx]
            payload = item['payload']
            
            # Calculate similarity to first item
            first_vector_idx = first_item['index']
            current_vector_idx = item['index']
            similarity_score = float(similarity_matrix[first_vector_idx][current_vector_idx])
            
            property_entry = {
                "source": payload.get('source', ''),
                "property": {
                    "id": payload.get('main_id', ''),
                    "sid": payload.get('sub_id', 0),
                    "property_specification": payload.get('property_specification', ''),
                    "uppaal_formula": payload.get('uppaal_formula', ''),
                    "reason": ""
                },
                "similarity_to_first": similarity_score
            }
            cluster_entry["properties"].append(property_entry)
        
        output_data.append(cluster_entry)
    
    # Determine output file path
    if output_file is None:
        output_dir = os.path.join(DTMC, "pyscript/LLM/property_extraction/data")
        os.makedirs(output_dir, exist_ok=True)
        if fc_type == "ardupilot":
            output_file = os.path.join(output_dir, f"copter_property_clusters.json")
        elif fc_type == "px4":
            output_file = os.path.join(output_dir, f"px4_property_clusters.json")
    
    # Write to JSON file
    with open(output_file, 'w', encoding='utf-8') as f:
        json.dump(output_data, f, indent=4, ensure_ascii=False)
    
    print(f"Clustering results exported to: {output_file}")
    print(f"Total clusters: {len(output_data)}")
    print(f"Threshold used: {threshold}")
    
    return output_file

def semantic_textual_similarity(fc_type: str, host: str = "localhost", port: int = 6333, threshold = 0.9):
    # Disable sklearn FutureWarnings
    import warnings
    warnings.filterwarnings("ignore", category=FutureWarning, module="sklearn")
    
    # Get vectors and payload mapping from Qdrant
    vectors, index_to_payload_mapping = get_vectors_from_qdrant(fc_type, host, port)
    
    # Noise reduction
    clusterer = hdbscan.HDBSCAN(
        min_cluster_size=2,
        min_samples=1,
        cluster_selection_epsilon=0.00,
        metric='cosine',
        algorithm='generic',
        gen_min_span_tree=True  # Enable for DBCV calculation
    )
    cluster_labels_hdbscan = clusterer.fit_predict(vectors)
    non_noise_indices = [i for i, label in enumerate(cluster_labels_hdbscan) if label != -1]
    
    if len(non_noise_indices) == 0:
        filtered_vectors = vectors
        filtered_index_to_payload_mapping = index_to_payload_mapping
        index_mapping = {i: i for i in range(len(vectors))}
    else:
        filtered_vectors = vectors[non_noise_indices]
        filtered_index_to_payload_mapping = {i: index_to_payload_mapping[original_idx] 
                                           for i, original_idx in enumerate(non_noise_indices)}
        index_mapping = {i: non_noise_indices[i] for i in range(len(non_noise_indices))}
        
    # Calculate cosine similarity matrix on filtered vectors
    similarity_matrix = cosine_similarity(filtered_vectors)

    # Get the number of filtered vectors
    n = len(filtered_vectors)
    
    # Initialize clusters - each item starts in its own cluster
    clusters = {i: [i] for i in range(n)}
    cluster_id_map = {i: i for i in range(n)}  # Maps item index to cluster id
    
    # Find similar pairs from upper triangle (excluding diagonal)
    for i in range(n):
        for j in range(i + 1, n):  # Upper triangle, excluding diagonal
            if similarity_matrix[i][j] >= threshold:
                # Merge clusters if similarity is above threshold
                cluster_i = cluster_id_map[i]
                cluster_j = cluster_id_map[j]
                
                if cluster_i != cluster_j:
                    # Merge cluster_j into cluster_i
                    clusters[cluster_i].extend(clusters[cluster_j])
                    
                    # Update cluster mapping for all items in cluster_j
                    for item in clusters[cluster_j]:
                        cluster_id_map[item] = cluster_i
                    
                    # Remove the merged cluster
                    del clusters[cluster_j]
    
    # Convert to final format: list of clusters with their payloads
    result_clusters = []
    cluster_labels = np.full(n, -1, dtype=int)  # Initialize with -1 (noise/unclustered)
    
    # Assign cluster labels for sklearn compatibility
    label_counter = 0
    for cluster_id, item_indices in clusters.items():
        cluster_data = {
            'cluster_id': cluster_id,
            'size': len(item_indices),
            'items': []
        }
        
        # Assign same label to all items in this cluster
        for idx in item_indices:
            cluster_labels[idx] = label_counter
            cluster_data['items'].append({
                'index': idx,
                'payload': filtered_index_to_payload_mapping[idx]
            })
        
        result_clusters.append(cluster_data)
        label_counter += 1
    
    return filtered_vectors, result_clusters, cluster_labels, similarity_matrix

def threshold_evaluation(fc_type: str, host: str = "localhost", port: int = 6333): 

    thresholds = []
    silhouette_scores = []
    
    print(f"{'threshold':<15}{'silhouette':<15}")

    threshold_range = np.concatenate([
        # np.arange(0.7, 0.85, 0.05),
        np.arange(0.7, 0.99, 0.01)
    ])

    for threshold in threshold_range:
        vectors, clusters, cluster_labels, similarity_matrix = semantic_textual_similarity(fc_type, host, port, threshold)
        
        # check clusters
        n_clusters = len(set(cluster_labels))
        
        thresholds.append(threshold)
        
        # Only calculate metrics when there are more than 1 cluster and less than all points
        if n_clusters > 1 and n_clusters < len(vectors):
            try:
                # Calculate Silhouette Coefficient, CH Index, DB Index
                silhouette_avg = silhouette_score(vectors, cluster_labels, metric='cosine')
                silhouette_scores.append(silhouette_avg)

                print(f"{threshold:<15.2f}{silhouette_avg:<15.4f}")
            except Exception as e:
                silhouette_scores.append(None)
                print(f"{threshold:.2f}\t\t{n_clusters}\t\tError: {str(e)}")
        else:
            silhouette_scores.append(None)
            print(f"{threshold:<15.2f}{'N/A':<15}")
    
def create_vector_db(deepseek_json, gemini_json, fc_type: str, host: str = "localhost", port: int = 6333):
    """Create vector database using Qdrant for storing property embeddings.
    
    Args:
        deepseek_json: JSON data from DeepSeek model
        gemini_json: JSON data from Gemini model  
        fc_type: Type of flight control system ("ardupilot" or "px4")
        host: Qdrant server host
        port: Qdrant server port
    """
    qdrant_client = QdrantClient(host=host, port=port)
    if fc_type == "ardupilot":
        collection_name = "ardupilot_wiki"
    elif fc_type == "px4":
        collection_name = "px4_wiki"
    else:
        raise ValueError("fc_type must be ardupilot or px4")
    
    # Extract properties from both JSON sources
    def extract_properties(json_data, source_name):
        properties = []
        for item in json_data:
            main_id = item['id']
            for sub_id, prop in enumerate(item.get('properties', [])):
                if prop and isinstance(prop, dict):
                    prop_copy = prop.copy()
                    prop_copy.update({
                        'id': main_id,
                        'sid': sub_id,
                        'source': source_name
                    })
                    properties.append(prop_copy)
        return properties
    
    # Filter out invalid properties
    deepseek_filtered = remove_illegal_items(deepseek_json)
    gemini_filtered = remove_illegal_items(gemini_json)
    
    # Extract properties
    deepseek_props = extract_properties(deepseek_filtered, "deepseek")
    gemini_props = extract_properties(gemini_filtered, "gemini")
    all_props = deepseek_props + gemini_props
    
    click.echo(f"Processing {len(deepseek_props)} properties from DeepSeek and {len(gemini_props)} properties from Gemini")
    
    # Create or recreate collection
    try:
        qdrant_client.delete_collection(collection_name)
        click.echo(f"Deleted existing collection: {collection_name}")
    except Exception:
        click.echo("Collection doesn't exist")
        pass  # Collection doesn't exist
    
    # Create collection with vector configuration
    qdrant_client.create_collection(
        collection_name=collection_name,
        vectors_config=VectorParams(size=1024, distance=Distance.COSINE)  # mxbai-embed-large produces 1024-dim vectors
    )
    click.echo(f"Created collection: {collection_name}")
    
    # Generate embeddings and store in Qdrant
    points = []
    click.echo("Generating embeddings and preparing data...")
    
    with tqdm(total=len(all_props), desc="Processing properties") as pbar:
        for idx, prop in enumerate(all_props):
            property_text = prop.get('property_specification', '')
            if not property_text or property_text in ["No", "N/A", "no", "n/a"]:
                pbar.update(1)
                continue
                
            # Generate embedding using ollama
            try:
                embedding = ollama.embed(model="mxbai-embed-large", input=property_text)["embeddings"][0]
                
                # Create point for Qdrant
                point = PointStruct(
                    id=idx,
                    vector=embedding,
                    payload={
                        "property_specification": property_text,
                        "source": prop.get('source', ''),
                        "main_id": prop.get('id', ''),
                        "sub_id": prop.get('sid', 0),
                        "uppaal_formula": prop.get('uppaal_formula', ''),
                        "flight_control_type": fc_type
                    }
                )
                points.append(point)
                
            except Exception as e:
                click.echo(f"Error generating embedding for property {idx}: {e}")
                
            pbar.update(1)
    
    # Upload points to Qdrant in batches to avoid payload size limit
    if points:
        batch_size = 100  # Adjust batch size to stay under 32MB limit
        total_points = len(points)
        click.echo(f"Uploading {total_points} points to Qdrant in batches of {batch_size}...")
        
        for i in range(0, total_points, batch_size):
            batch = points[i:i + batch_size]
            try:
                qdrant_client.upsert(
                    collection_name=collection_name,
                    points=batch
                )
                click.echo(f"Uploaded batch {i//batch_size + 1}/{(total_points + batch_size - 1)//batch_size} ({len(batch)} points)")
            except Exception as e:
                click.echo(f"Error uploading batch {i//batch_size + 1}: {e}")
                raise
        
        click.echo(f"Successfully created vector database with {total_points} property embeddings")
    else:
        click.echo("No valid properties found to store in vector database")
    
    return collection_name

@click.command(name='text_embedding')
@click.option('-t', '--fc-type', type=click.Choice(['ardupilot', 'px4'], case_sensitive=False), 
              required=True, help="flight controls you want to build: ardupilot, px4")
def text_embedding(fc_type):
    total_start_time = time.time()
    if fc_type == 'ardupilot':
        deepseek = DTMC + '/pyscript/LLM/property_extraction/data/copter_deepseek_property_specifications.json'
        gemini = DTMC + '/pyscript/LLM/property_extraction/data/copter_openrouter_property_specifications.json'
    elif fc_type == 'px4':
        deepseek = DTMC + '/pyscript/LLM/property_extraction/data/px4_deepseek_property_specifications.json'
        gemini = DTMC + '/pyscript/LLM/property_extraction/data/px4_openrouter_property_specifications.json'
    else:
        click.echo("fc_type must be ardupilot or px4")
        return

    try:
        # Load JSON files
        click.echo(f"Loading DeepSeek properties from: {deepseek}")
        with open(deepseek, 'r', encoding='utf-8') as f:
            deepseek_data = json.load(f)
        
        click.echo(f"Loading Gemini properties from: {gemini}")
        with open(gemini, 'r', encoding='utf-8') as f:
            gemini_data = json.load(f)
        
        create_vector_db(deepseek_data, gemini_data, fc_type)
    except Exception as e:
        click.echo(f"Error in create_vector_db: {e}")
        import traceback
        click.echo(traceback.format_exc())
        raise

##########################################################################################################
##########################################################################################################
##########################################################################################################

def extract_useit_items(json_data):
    """
    Extract items with 'useit' field set to True and construct new format.
    
    Args:
        json_data (list): List of clusters with properties
        
    Returns:
        list: Filtered list containing only items with useit=True in new format
    """
    result = []
    
    for cluster in json_data:
        cluster_id = cluster.get('id', 0)
        for prop_item in cluster.get('properties', []):
            if prop_item.get('useit') == True:
                prop = prop_item.get('property')
                if prop:
                    # Construct new id format: cluster_id-property_id-sid
                    property_id = prop.get('id', 0)
                    sid = prop.get('sid', 0)
                    new_id = f"{cluster_id}-{property_id}-{sid}"
                    
                    # Create new property format
                    new_prop = {
                        'id': new_id,
                        'property_specification': prop.get('property_specification', ''),
                        'uppaal_formula': prop.get('uppaal_formula', '')
                    }
                    result.append(new_prop)

    # Sort by the original property id for consistency
    result.sort(key=lambda x: int(x.get('id', '0-0-0').split('-')[1]))

    return result

@click.command(name='extract_useit')
@click.option('-i', '--input', required=True, type=str, help='Path to input JSON file')
@click.option('-o', '--output', required=True, type=str, help='Path to output JSON file')
def extract_useit(input, output):
    """
    Extract properties with useit=True from a JSON file and save to a new file without sid field.
    """
    try:
        click.echo(f"Reading input file: {input}")
        with open(input, 'r', encoding='utf-8') as f:
            json_data = json.load(f)
        
        useit_items = extract_useit_items(json_data)
        
        click.echo(f"Saving results to: {output}")
        with open(output, 'w', encoding='utf-8') as f:
            json.dump(useit_items, f, indent=4)
        
        click.echo(f"Successfully extracted {len(useit_items)} items with useit=True")
    
    except Exception as e:
        click.echo(f"Error during extraction: {str(e)}")
        import traceback
        click.echo(traceback.format_exc())
        raise

# @click.group()
# def cli():
#     pass

# cli.add_command(uav_doc_extract)
# cli.add_command(reorganize_csv)
# cli.add_command(text_embedding)
# cli.add_command(extract_useit)

# if __name__ == '__main__':
#     cli()

