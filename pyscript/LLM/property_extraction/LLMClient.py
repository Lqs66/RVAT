import os
import click
import json
import time
import tempfile
import shutil
from openai import OpenAI
import pandas as pd
from dataclasses import dataclass

DTMC = os.getenv('DTMC')
if DTMC is None:
    print("Please set DTMC environment variable to the root of the project")
    exit(1)

@dataclass
class Recipe:
    natural_language: str
    uppaal_formula: str
    reason: str

class DeepSeekConfig:
    api_key = os.getenv("DEEPSEEK_API_KEY")
    base_url = "https://api.deepseek.com"
    model = "deepseek-reasoner"
    
    @classmethod
    def validate(cls):
        """Validate DeepSeek configuration"""
        if not cls.api_key:
            raise ValueError("DEEPSEEK_API_KEY environment variable is not set, cannot use DeepSeek API")
        return True

class OpenRouterConfig:
    api_key = os.getenv("OPENROUTER_API_KEY")
    base_url = "https://openrouter.ai/api/v1"
    model = "google/gemini-2.5-flash"
    
    @classmethod
    def validate(cls):
        """Validate OpenRouter configuration"""
        if not cls.api_key:
            raise ValueError("OPENROUTER_API_KEY environment variable is not set, cannot use OpenRouter API")
        return True

Finder_Prompt_txt = os.path.join(DTMC, 'pyscript', 'LLM', 'property_extraction', 'data', 'Finder_Prompt.txt')
if not os.path.exists(Finder_Prompt_txt):
    print(f"File {Finder_Prompt_txt} is not existed")
    exit(1)
Finder_Prompt = open(Finder_Prompt_txt, 'r').read()

def llm_extract_property_specifications(
        api_key : str,
        base_url : str,
        model : str,    
        target_content : str) -> str:
    """Call LLM API to extract property specifications from target content"""
    try:
        client = OpenAI(api_key=api_key, base_url=base_url)
        
        target_content = f"Here is the text to analyze: {target_content}"
        
        response = client.chat.completions.create(
            model=model,
            messages=[{"role": "system", "content": Finder_Prompt},
                    {"role": "user", "content": target_content}]
        )

        print(response.choices[0].message.content)
        llm_response = response.choices[0].message.content
        # Remove markdown formatting
        llm_response = llm_response.replace('```json', '').replace('```', '').strip()
        json_data = json.loads(llm_response)
        recipes = []
        for item in json_data:
            recipes.append(Recipe(**item))
        return recipes
    except json.JSONDecodeError as e:
        print(f"JSON parsing error: {e}")
        return []
    except Exception as e:
        print(f"Handle {target_content} error: {e}")
        return []

# We use a temporary file to save results and then atomically replace the target file
def save_json(results, filepath):
    with tempfile.NamedTemporaryFile(mode='w+', delete=False) as temp_file:
        temp_filename = temp_file.name
        json.dump(results, temp_file, indent=2, ensure_ascii=False)
    
    shutil.move(temp_filename, filepath)

@click.command(name='extract_property_specifications')
@click.option('-f', '--file', required=True, type=str, help='Path of documentation to extract property specifications')
@click.option('-r', '--resume', is_flag=True, help='Resume from last processed ID')
@click.option('-t', '--timeout', type=int, help='Maximum execution time in minutes')
@click.option('-m', '--model', type=click.Choice(['deepseek', 'openrouter']), required=True,
              help='LLM model to use (deepseek or openrouter)')

def extract_property_specifications(file, resume, timeout, model):
    """Extract property specifications from target document"""
    
    type = file.split('/')[-1].split('.')[0]
    # Format output file and checkpoint file.
    output_file = os.path.join(DTMC, 'pyscript', 'LLM', 'property_extraction', 'data', f"{type}_{model}_property_specifications.json")
    checkpoint_file = os.path.join(DTMC, 'pyscript', 'LLM', 'property_extraction', 'data', f"{type}_{model}_checkpoint.txt")
    
    # Access the correct config based on model choice and validate it
    if model == 'deepseek':
        config = DeepSeekConfig
        config.validate()
        print(f"Using DeepSeek model: {config.model}")
    else:  # model == 'openrouter'
        config = OpenRouterConfig
        config.validate()
        print(f"Using OpenRouter model: {config.model}")
        
    start_time = time.time()
    
    # Initialize results from existing file if it exists
    all_results = []
    last_processed_id = None
    
    if os.path.exists(output_file):
        try:
            with open(output_file, 'r', encoding='utf-8') as f:
                all_results = json.load(f)
            print(f"Loaded {len(all_results)} existing results from {output_file}")
        except Exception as e:
            print(f"Error loading existing results: {e}")
    
    # Check for checkpoint if resuming
    if resume and os.path.exists(checkpoint_file):
        try:
            with open(checkpoint_file, 'r', encoding='utf-8') as f:
                last_processed_id = f.read().strip()
                if last_processed_id:
                    print(f"Resuming from ID: {last_processed_id}")
        except Exception as e:
            print(f"Error loading checkpoint: {e}")
    
    try:
        df = pd.read_csv(file)
        print(f"Success load file: {file}")
        print(f"Total rows: {len(df)}")
        
        # Get all processed IDs
        processed_ids = set(result["id"] for result in all_results)
        
        if last_processed_id:
            try:
                last_idx = df[df['id'] == int(last_processed_id)].index[0]
                df = df.iloc[last_idx+1:]
                print(f"Starting from row after ID {last_processed_id}, remaining rows: {len(df)}")
            except (IndexError, KeyError):
                print(f"Warning: Could not find ID {last_processed_id} in the file, abort execution")
                return
        
        last_successful_id = last_processed_id  # Record the last successful ID
        
        # extract (id, text) from each row
        for _, row in df.iterrows():
            # Check for timeout
            if timeout is not None and (time.time() - start_time) > (timeout * 60):
                print(f"Reached timeout limit of {timeout} minutes.")
                
                if all_results:
                    save_json(all_results, output_file)
                    # Save checkpoint - last successful ID
                    if last_successful_id is not None:
                        with open(checkpoint_file, 'w', encoding='utf-8') as f:
                            f.write(str(last_successful_id))
                        print(f"Checkpoint saved at ID: {last_successful_id}")
                    print(f"Saved current results. To continue, use the --resume option.")
                else:
                    print("No results to save.")              
                return
            
            id = row['id']
            
            # Check if this ID is already in results
            if id in processed_ids:
                print(f"Skipping already processed ID: {id}")
                continue
                
            file_content = row['text']
            print(f"Extracting property specifications from id: {id}")
            
            try:
                retry_count = 0
                max_retries = 10
                recipes = []
                
                while not recipes and retry_count < max_retries:
                    if retry_count > 0:
                        print(f"Retry API call (has tried {retry_count+1}/{max_retries} times)")
                    
                    recipes = llm_extract_property_specifications(
                        api_key=config.api_key,
                        base_url=config.base_url,
                        model=config.model,
                        target_content=file_content
                    )
                    retry_count += 1
                
                if not recipes:
                    raise Exception(f"Call LLM API failed after {max_retries} retries")
                
                # handle recipes
                id_to_properties = {}
                for recipe in recipes:
                    property_info = {
                        "property_specification": recipe.natural_language,
                        "uppaal_formula": recipe.uppaal_formula,
                        "reason": recipe.reason
                    }
                    
                    if id in id_to_properties:
                        id_to_properties[id]["properties"].append(property_info)
                    else:
                        id_to_properties[id] = {
                            "id": id,
                            "properties": [property_info]
                        }
                
                # Check if ID already exists in results, update if found
                found = False
                for i, result in enumerate(all_results):
                    if result["id"] == id:
                        all_results[i] = id_to_properties[id]
                        found = True
                        break
                
                # If ID not found, add to results
                if not found and id in id_to_properties:
                    all_results.append(id_to_properties[id])
                    processed_ids.add(id)
                
                save_json(all_results, output_file)
                
                # Update last successful ID
                last_successful_id = id
                
                # Only save checkpoint if ID was successfully processed
                with open(checkpoint_file, 'w', encoding='utf-8') as f:
                    f.write(str(last_successful_id))
                    
                print(f"Saved results after processing ID: {id}")
                print("--------------------------------------")
                
            except Exception as e:
                print(f"Error processing ID {id}: {e}")
                print(f"Checkpoint remains at last successful ID: {last_successful_id}")
                raise

        print(f"All items processed successfully!")
        print(f"Total results: {len(all_results)}")
        
        # Save final checkpoint
        if last_successful_id is not None:
            with open(checkpoint_file, 'w', encoding='utf-8') as f:
                f.write(str(last_successful_id))
            print(f"Final checkpoint saved at ID: {last_successful_id}")

    except Exception as e:
        print(f"Handle {file} error: {e}")

if __name__ == "__main__":
    extract_property_specifications()