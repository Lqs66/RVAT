import os
from .prompts import *
from pyscript.pyTools.utils import *

class Code_Snippet_List:
    def __init__(self):
        self.list = []

    def add(self, code, start_line, end_line, file_path):
        code = "\t\t" + code.replace("\n", "\n\t\t")
        code_snippet = {
            "code": code,
            "start_line": start_line,
            "end_line": end_line,
            "file_path": file_path
        }
        self.list.append(code_snippet)

    def __str__(self):
        result = ""
        for item in self.list:
            result += "[\n"
            result += "\t code: " + item["code"] + "\n"
            result += "\t start_line: " + str(item["start_line"]) + "\n"
            result += "\t end_line: " + str(item["end_line"]) + "\n"
            result += "\t file_path: " + item["file_path"] + "\n"
            if item != self.list[-1]:
                result += "],\n"
            else:
                result += "]\n"
        return result

class OpenRouterLLM:
    api_key = os.getenv("OPENROUTER_API_KEY")
    base_url = "https://openrouter.ai/api/v1"
    
    def __init__(self, model = "google/gemini-2.5-flash"):
        self.model = model
        if not self.api_key:
            raise ValueError("OPENROUTER_API_KEY environment variable is not set, cannot use OpenRouter API")

    def find_slice_criterias(self, formula, code_snippet_list: Code_Snippet_List):
        import json
        prompt = SC_GEN.format(formula=formula, code_snippet_list=str(code_snippet_list))
        from openai import OpenAI
        client = OpenAI(
            api_key=self.api_key,
            base_url=self.base_url,
        )
        
        max_retries = 3
        for attempt in range(max_retries):
            try:
                completion = client.chat.completions.create(
                    model=self.model,
                    messages=[
                        {
                            "role": "user",
                            "content": [
                                {
                                    "type": "text",
                                    "text": prompt,
                                }
                            ]
                        }
                    ],
                    response_format={
                        "type": "json_object"
                    }
                )

                response = completion.choices[0].message.content
                
                # Validate if response is valid JSON format
                try:
                    json_response = json.loads(response)
                    print(f"Successfully obtained valid JSON response (attempt {attempt + 1}/{max_retries})")
                    print(response)
                    return json_response
                except json.JSONDecodeError as e:
                    print(f"JSON parsing failed (attempt {attempt + 1}/{max_retries}): {e}")
                    print(f"Raw response: {response}")
                    if attempt == max_retries - 1:
                        print("Maximum retry attempts reached, returning raw response")
                        return response
                    else:
                        print("Retrying...")
                        continue
                        
            except Exception as e:
                print(f"API call failed (attempt {attempt + 1}/{max_retries}): {e}")
                if attempt == max_retries - 1:
                    raise e
                else:
                    print("Retrying...")
                    continue
        
        return None


