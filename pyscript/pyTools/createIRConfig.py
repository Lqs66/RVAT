import yaml
import click
from collections import OrderedDict
from pyscript.pyTools.utils import *

properties_config = DTMC + "/configs/properties_config.yml"

# Custom YAML loader to maintain original order
class OrderedLoader(yaml.SafeLoader):
    pass

def construct_mapping(loader, node):
    loader.flatten_mapping(node)
    return OrderedDict(loader.construct_pairs(node))

OrderedLoader.add_constructor(yaml.resolver.BaseResolver.DEFAULT_MAPPING_TAG, construct_mapping)

# Custom YAML Dumper to add list indentation
class CustomDumper(yaml.SafeDumper):
    def increase_indent(self, flow=False, indentless=False):
        return super(CustomDumper, self).increase_indent(flow, False)

# Custom YAML representer to maintain OrderedDict order
def represent_ordereddict(dumper, data):
    return dumper.represent_mapping('tag:yaml.org,2002:map', data.items())

CustomDumper.add_representer(OrderedDict, represent_ordereddict)

@click.command(name="create_IR_config")
@click.option("--property", "-p", type=str, required=True, help="The property to be verified.")
def create_IR_config(property: str):
    """
    Create the IR_config.yml configuration file for the property to be verified.
    """
    if DTMC is None:
        ERROR("Please set DTMC environment variable to the root of the project")
        return

    with open(properties_config, 'r') as stream:
        try:
            # Use custom loader to maintain original order
            parsed_data = yaml.load(stream, Loader=OrderedLoader)
            if property not in parsed_data:
                ERROR(f"Property '{property}' not found in {properties_config}")
                return
            
            config = parsed_data[property]
            
            # Create ordered dictionary, first add property field, then maintain original config order
            ordered_config = OrderedDict()
            ordered_config["property"] = property
            
            # Directly add all fields from original config, maintaining their original order
            for key, value in config.items():
                ordered_config[key] = value

            with open(DTMC + "/configs/IR_config.yml", 'w') as out_stream:
                yaml.dump(ordered_config, out_stream, Dumper=CustomDumper,
                         default_flow_style=False, sort_keys=False, 
                         allow_unicode=True, indent=2)
            INFO(f"IR_config.yml for property '{property}' created successfully.")
        except yaml.YAMLError as exc:
            ERROR(exc)