import yaml
import os
from pyscript.pyTools.utils import *

__all__ = ["parse_IR_config"]

def parse_IR_config():
    if DTMC is None:
        ERROR("Please set DTMC environment variable to the root of the project")
        return None
    IR_config = DTMC + "/configs/IR_config.yml"
    
    # Check if the file exists
    if not os.path.exists(IR_config):
        return None
    
    with open(IR_config, 'r') as stream:
        try:
            parsed_data = yaml.safe_load(stream)
            params = tuple(IRConfig.FuncParamValue(**param) for param in parsed_data.get("params", []))

            def parse_offset(data):
                if isinstance(data, dict):
                    return IRConfig.Offset(**data)
                return data

            # Check if queries field exists
            if "queries" not in parsed_data:
                ERROR("'queries' field not found in IR_config.yml")
                return None
            
            qs = parsed_data["queries"]
            registerQueries = tuple()
            stackQueries = tuple()
            globalQueries = tuple()
            for q in qs:
                scope = q["scope"]
                if scope == "register":
                    registerQueries += (IRConfig.RegisterQuery(
                        pname=q.get("pname"),
                        cname=q.get("cname"),
                        file=q.get("file"),
                        line=q.get("line"),
                        col=q.get("col"),
                        type=q.get("type"),
                    ),)   
                elif scope == "stack":
                    field= q.get("field")
                    # field format: stackVarName + offset, such as "a + 32".
                    # We need to split it.
                    if field:
                        parts = field.split('+')
                        stackVarName = parts[0].strip()
                        offset = int(parts[1].strip()) if len(parts) > 1 else 0
                    else:
                        ERROR(f"Field is missing in stack query: {q}")
                        exit(1)
                    stackQueries += (IRConfig.StackQuery(
                        pname=q.get("pname"),
                        cname=q.get("cname"),
                        file=q.get("file"),
                        line=q.get("line"),
                        stackVarName=stackVarName,
                        offset=offset,
                        type=q.get("type"),
                    ),)
                elif scope == "global":
                    field= q.get("field")
                    # field format: globalVarName + offset, such as "heap + 32".
                    # We need to split it.
                    if field:
                        parts = field.split('+')
                        globalVarName = parts[0].strip()
                        offset = int(parts[1].strip()) if len(parts) > 1 else 0
                    else:
                        ERROR(f"Field is missing in global query: {q}")
                        exit(1)
                    globalQueries += (IRConfig.GlobalQuery(
                        pname=q.get("pname"),
                        cname=q.get("cname"),
                        globalVarName=globalVarName,
                        offset=offset,
                        type=q.get("type"),
                    ),)
                else:
                    ERROR(f"Unknown query scope: {scope}")
                    exit(1)

            return IRConfig(
                property=parsed_data.get("property"),
                flightControl=parsed_data.get("flightControl"),
                LLVM_IR=parsed_data.get("LLVM_IR"),
                call_depth=parsed_data.get("call_depth"),
                entrypoints=tuple(parsed_data.get("entrypoints", [])),
                params=params,
                isTime=parsed_data.get("isTime"),
                registerQueries=registerQueries,
                stackQueries=stackQueries,
                globalQueries=globalQueries,
            )

        except yaml.YAMLError as exc:
            ERROR(exc)
            return None
