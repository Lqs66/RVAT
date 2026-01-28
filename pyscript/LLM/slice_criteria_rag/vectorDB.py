import os
import uuid
import requests
import time
from collections import deque

from typing import List, Dict, Any, Optional
from qdrant_client import QdrantClient # pip install qdrant_client
from qdrant_client.models import Distance, VectorParams, PointStruct
from .chunking import Chunk
from tqdm import tqdm

URL = "https://api.siliconflow.cn/v1/embeddings"
QWEN3_API_KEY = os.getenv("QWEN3_API_KEY")
if QWEN3_API_KEY is None:
    raise ValueError("QWEN3_API_KEY not found in environment variables")

DTMC = os.getenv("DTMC")

class Qwen3EmbeddingFunction:
    """Custom embedding function using SiliconFlow Qwen3 embedding model"""
    
    def __init__(self, model_name: str = "Qwen/Qwen3-Embedding-8B", max_rpm: int = 2000, max_tpm: int = 1000000):
        self.model_name = model_name
        self.api_url = URL
        self.api_key = QWEN3_API_KEY
        self.headers = {
            "Authorization": f"Bearer {self.api_key}",
            "Content-Type": "application/json"
        }
        
        # Store rate limiting parameters for reference
        self.max_rpm = max_rpm
        self.max_tpm = max_tpm
        
        # Initialize rate limiting tracking
        self.request_times = deque()
        self.token_usage = deque()
    

    def embed_documents(self, documents: List[str]) -> List[List[float]]:
        all_embeddings = []
        
        for i, doc in enumerate(documents):
            embedding = self._embed_single_document_with_retry(doc)
            all_embeddings.append(embedding)
        
        return all_embeddings
    
    def _embed_single_document_with_retry(self, document: str, max_retries: int = 3) -> List[float]:
        """Single document embedding with retry mechanism and rate limiting"""
        estimated_tokens = self._estimate_tokens(document)
        
        for attempt in range(max_retries):
            try:
                # Check rate limits before making request
                self._check_rate_limits(estimated_tokens)
                
                # Make embedding request
                payload = {
                    "model": self.model_name,
                    "input": [document]
                }
                
                response = requests.post(self.api_url, json=payload, headers=self.headers, timeout=30)
                
                if response.status_code == 200:
                    result = response.json()
                    
                    # Update token usage with actual usage if available
                    if 'usage' in result and 'total_tokens' in result['usage']:
                        actual_tokens = result['usage']['total_tokens']
                        # Update the last token usage record with actual tokens
                        if self.token_usage:
                            self.token_usage[-1] = (self.token_usage[-1][0], actual_tokens)
                    
                    if 'data' in result and len(result['data']) > 0:
                        return result['data'][0]['embedding']
                    else:
                        raise ValueError("API returned invalid data format")
                else:
                    print(f"Embedding request failed with status {response.status_code}: {response.text}")
                    if attempt < max_retries - 1:
                        wait_time = 2 ** attempt
                        print(f"Retrying in {wait_time} seconds... (attempt {attempt + 1}/{max_retries})")
                        time.sleep(wait_time)
                    
            except Exception as e:
                print(f"Error during embedding request: {e}")
                if attempt < max_retries - 1:
                    wait_time = 2 ** attempt
                    print(f"Retrying in {wait_time} seconds... (attempt {attempt + 1}/{max_retries})")
                    time.sleep(wait_time)
        
        # If all retries failed, return zero vector as placeholder
        print(f"All embedding attempts failed for document: {document[:100]}...")
        print("Returning zero vector as placeholder")
        return [0.0] * 4096
    
    def embed_query(self, query: str) -> List[float]:
        """Generate embedding for a single query with rate limiting and retry"""
        return self._embed_single_document_with_retry(query)
    
    def _estimate_tokens(self, text: str) -> int:
        """Estimate the number of tokens in a text string"""
        # Simple estimation: roughly 4 characters per token for Chinese/English mixed text
        return max(1, len(text) // 4)
    
    def _check_rate_limits(self, estimated_tokens: int):
        """Check and enforce rate limits for RPM and TPM"""
        while True:
            current_time = time.time()
            
            # Clean old records (older than 1 minute)
            while self.request_times and current_time - self.request_times[0] > 60:
                self.request_times.popleft()
            while self.token_usage and current_time - self.token_usage[0][0] > 60:
                self.token_usage.popleft()
            
            # Check RPM limit
            if len(self.request_times) >= self.max_rpm:
                sleep_time = 60 - (current_time - self.request_times[0])
                if sleep_time > 0:
                    print(f"Rate limit reached. Sleeping for {sleep_time:.2f} seconds...")
                    time.sleep(sleep_time)
                    continue  # Continue the loop to recheck
            
            # Check TPM limit
            current_tokens = sum(tokens for _, tokens in self.token_usage)
            if current_tokens + estimated_tokens > self.max_tpm:
                sleep_time = 60 - (current_time - self.token_usage[0][0])
                if sleep_time > 0:
                    print(f"Token limit reached. Sleeping for {sleep_time:.2f} seconds...")
                    time.sleep(sleep_time)
                    continue  # Continue the loop to recheck
            
            # If we reach here, rate limits are satisfied
            break
        
        # Record this request
        self.request_times.append(current_time)
        if estimated_tokens > 0:
            self.token_usage.append((current_time, estimated_tokens))


class QdrantVectorDB:
    """Vector database for storing and retrieving code chunks using Qdrant"""
    
    def __init__(self, collection_name: str = "code_chunks", host: str = "localhost", port: int = 6333, max_rpm: int = 2000, max_tpm: int = 1000000):
        self.collection_name = collection_name
        
        # Initialize Qdrant client with timeout settings
        self.client = QdrantClient(
            host=host, 
            port=port,
            timeout=300  # 5 minutes timeout for large uploads
        )
        
        # Initialize Qwen3 embedding function
        self.embedding_function = Qwen3EmbeddingFunction(max_rpm=max_rpm, max_tpm=max_tpm)
        
        # Store rate limiting parameters for reference
        self.max_rpm = max_rpm
        self.max_tpm = max_tpm
        
        # Fixed vector dimension
        self.vector_dimension = 4096
        
        # Initialize existing hashes set
        self.existing_hashes = set()
        
        # Check if collection exists and verify dimension
        try:
            collection_info = self.client.get_collection(collection_name)
            existing_dimension = collection_info.config.params.vectors.size
            if existing_dimension != self.vector_dimension:
                print(f"Warning: Existing collection '{collection_name}' has dimension {existing_dimension}, but expected {self.vector_dimension}")
                print(f"Consider deleting the collection or using a different collection name")
            # Load existing hashes into memory
            self._load_existing_hashes()
        except Exception:
            # Collection doesn't exist, create it with fixed dimension
            self.client.create_collection(
                collection_name=collection_name,
                vectors_config=VectorParams(size=self.vector_dimension, distance=Distance.COSINE)
            )
            print(f"Created new collection '{collection_name}' with dimension {self.vector_dimension}")

    def _load_existing_hashes(self):
        """Load all existing chunk hashes into memory"""
        try:
            # Scroll through all points to get existing hashes
            offset = None
            while True:
                scroll_result = self.client.scroll(
                    collection_name=self.collection_name,
                    limit=1000,  # Process in batches
                    offset=offset,
                    with_payload=["chunk_hash"]
                )
                
                points, next_offset = scroll_result
                
                # Process current batch
                for point in points:
                    if point.payload and "chunk_hash" in point.payload:
                        self.existing_hashes.add(point.payload["chunk_hash"])
                
                # Check if we've reached the end
                if next_offset is None or len(points) == 0:
                    break
                    
                offset = next_offset
            
            print(f"Loaded {len(self.existing_hashes)} existing hashes into memory")
        except Exception as e:
            print(f"Warning: Could not load existing hashes: {e}")
    
    def add_chunks(self, chunks: List[Chunk]) -> None:
        """Add chunks to the vector database"""
        if not chunks:
            return
        
        documents = []
        points = []
        new_chunks = []
        
        # Filter out chunks that already exist using in-memory hash set
        for chunk in chunks:
            chunk_hash = hash(chunk)
            if chunk_hash not in self.existing_hashes:
                new_chunks.append(chunk)
                documents.append(chunk.content)
                # Add to existing hashes immediately to avoid duplicates in this batch
                self.existing_hashes.add(chunk_hash)
            else:
                print(f"Chunk with hash {chunk_hash} already exists, skipping")
        
        if not new_chunks:
            print("No new chunks to add")
            return
        print(f"Prepare to add {len(new_chunks)} new chunks...")
        try:
            # Generate embeddings for new documents using embedding function
            embeddings = self.embedding_function.embed_documents(documents)
            
            # Create points for Qdrant
            for i, chunk in enumerate(new_chunks):
                # Use chunk's deterministic hash method for consistent IDs
                chunk_hash = hash(chunk)
                # Use UUID for point_id to ensure uniqueness
                point_id = str(uuid.uuid4())
                
                payload = {
                    "content": chunk.content,
                    "start_line": chunk.start_line,
                    "end_line": chunk.end_line,
                    "file_path": chunk.file_path,
                    "chunk_hash": chunk_hash  # Store hash for future checks
                }
                
                point = PointStruct(
                    id=point_id,
                    vector=embeddings[i],
                    payload=payload
                )
                points.append(point)
            
            # Upload points to Qdrant in batches to avoid timeout
            batch_size = 100  # Process in smaller batches
            total_points = len(points)
            
            # Use tqdm progress bar for better visualization
            with tqdm(total=total_points, desc="Uploading chunks", unit="chunks") as pbar:
                for i in range(0, total_points, batch_size):
                    batch_points = points[i:i + batch_size]
                    try:
                        self.client.upsert(
                            collection_name=self.collection_name,
                            points=batch_points
                        )
                        pbar.update(len(batch_points))
                        pbar.set_postfix({"batch": f"{i//batch_size + 1}/{(total_points + batch_size - 1)//batch_size}"})
                    except Exception as batch_error:
                        print(f"\nError uploading batch {i//batch_size + 1}: {batch_error}")
                        # Remove hashes for failed batch
                        for j in range(len(batch_points)):
                            chunk_idx = i + j
                            if chunk_idx < len(new_chunks):
                                chunk_hash = hash(new_chunks[chunk_idx])
                                self.existing_hashes.discard(chunk_hash)
                        raise batch_error
            
            print(f"Successfully added {len(new_chunks)} new chunks to vector database")
        except Exception as e:
            print(f"Error adding chunks to database: {e}")
            # If upload failed, remove the hashes we added to existing_hashes
            for chunk in new_chunks:
                chunk_hash = hash(chunk)
                self.existing_hashes.discard(chunk_hash)
    
    def search(self, query: str, n_results: int = 5, where: Optional[Dict[str, Any]] = None) -> List[Dict[str, Any]]:
        """Search for similar chunks in the database"""
        try:
            # Generate embedding for the query using embedding function
            query_embedding = self.embedding_function.embed_query(query)
            
            # Search in Qdrant
            search_results = self.client.search(
                collection_name=self.collection_name,
                query_vector=query_embedding,
                limit=n_results,
                query_filter=where
            )
            
            # Format results
            formatted_results = []
            for result in search_results:
                formatted_result = {
                    "id": result.id,
                    "content": result.payload.get("content", ""),
                    "metadata": {
                        "start_line": result.payload.get("start_line"),
                        "end_line": result.payload.get("end_line"),
                        "file_path": result.payload.get("file_path"),
                        "token_count": result.payload.get("token_count")
                    },
                    "score": result.score
                }
                formatted_results.append(formatted_result)
            
            return formatted_results
        except Exception as e:
            print(f"Error searching database: {e}")
            return []
    
    def query(self, query_text: str, top_k: int = 5):
        """Query method that returns results function"""
        try:
            # Generate embedding for the query using embedding function
            query_embedding = self.embedding_function.embed_query(query_text)
            
            # Search in Qdrant
            search_results = self.client.search(
                collection_name=self.collection_name,
                query_vector=query_embedding,
                limit=top_k
            )
            
            return search_results
        except Exception as e:
            print(f"Error querying database: {e}")
            return []
    
    def display_vectors_with_payload(self, limit: int = 10) -> None:
        """Display vectors and their corresponding payload information in the vector database"""
        try:
            # Get all points in the collection
            scroll_result = self.client.scroll(
                collection_name=self.collection_name,
                limit=limit,
                with_payload=True,
                with_vectors=True
            )
            
            points = scroll_result[0]  # scroll returns (points, next_page_offset)
            
            if not points:
                print("No data in vector database")
                return
            
            print(f"\n=== Vector Database Content (showing first {len(points)} records) ===")
            for i, point in enumerate(points, 1):
                print(f"\n--- Record {i} ---")
                print(f"ID: {point.id}")
                print(f"Vector dimension: {len(point.vector) if point.vector else 'N/A'}")
                print(f"First 5 dimensions: {point.vector[:5] if point.vector and len(point.vector) >= 5 else point.vector}")
                
                if point.payload:
                    print("Payload information:")
                    for key, value in point.payload.items():
                        if key == "content":
                            # Truncate content to avoid overly long output
                            content_preview = value[:100] + "..." if len(value) > 100 else value
                            print(f"  {key}: {content_preview}")
                        else:
                            print(f"  {key}: {value}")
                else:
                    print("Payload: None")
                    
        except Exception as e:
            print(f"Error displaying vector information: {e}")
    
    def delete_collection(self, collection_name: Optional[str] = None) -> bool:
        """Delete the specified collection
        
        Args:
            collection_name: Name of the collection to delete, if None, delete current collection
            
        Returns:
            bool: True if deletion successful, False if failed
        """
        target_collection = collection_name if collection_name is not None else self.collection_name
        
        try:
            # Check if collection exists
            try:
                self.client.get_collection(target_collection)
            except Exception:
                print(f"Collection '{target_collection}' does not exist")
                return False
            
            # Delete collection
            self.client.delete_collection(collection_name=target_collection)
            print(f"Successfully deleted collection: {target_collection}")
            
            return True
            
        except Exception as e:
            print(f"Error deleting collection: {e}")
            return False