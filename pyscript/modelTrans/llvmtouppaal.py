# import external.pyuppaal.pyuppaal as pyuppaal
from .external.pyuppaal.pyuppaal import *
from ctypes import *
import time
from collections import deque
from ..pyTools.parseConfig import *
from ..pyTools.utils import *
import yaml

__all__ = ["run"]

timeBounds = None
verificationConfig = None

def parse_verification_config(property_name):
    """
    Parse verification configuration for a specific property.
    
    Parameters:
        property_name: The name of the property to verify
        
    Returns:
        dict: Configuration dictionary with 'vars', 'clocks', 'updates', 'locations', and 'uppaal_formula'
        - 'vars': List of variable declarations, can include initialization (e.g., 'circle_acceleration_leq_2 = true')
        - 'clocks': List of clock variable names (e.g., 'recover_time')
        - 'updates': List of update expressions
        - 'locations': List of location alias mappings (e.g., 'program_exit = exit')
        - 'uppaal_formula': UPPAAL query formula (symbolic names will be replaced with template.location)
    """
    if DTMC is None:
        ERROR("Please set DTMC environment variable to the root of the project")
        exit(1)

    verifications_config_path = DTMC + "/configs/verifications.yml"
    if not os.path.exists(verifications_config_path):
        ERROR(f"Verification config file not found: {verifications_config_path}")
        exit(1)
    
    with open(verifications_config_path, 'r') as f:
        config = yaml.safe_load(f)
    
    if property_name not in config:
        WARNING(f"Property {property_name} not found in verification config, using empty config")
        return {'vars': [], 'clocks': [], 'updates': [], 'locations': [], 'uppaal_formula': ''}
    
    return config[property_name]

def getTimeBoundByBBNum(bbNum):
    """
    Get time bounds for a specific basic block number.
    
    Parameters:
        bbNum: The basic block number (can be string or int)
        
    Returns:
        tuple: (lower, upper) or None
    """
    if timeBounds is None:
        return None

    # convert bbNumxx to xx
    if isinstance(bbNum, str) and bbNum.startswith("bbNum"):
        bbNum = bbNum[5:]
    
    # Convert bbNum to int
    try:
        bbNum_int = int(bbNum) if isinstance(bbNum, str) else bbNum
    except (ValueError, TypeError):
        return None
    
    if bbNum_int not in timeBounds:
        return None
    
    # Get the time_constraint dictionary and return only lower and upper
    time_constraint = timeBounds[bbNum_int].get('time_constraint', None)
    if time_constraint is None:
        return None
    
    return (time_constraint['lower'], time_constraint['upper'])

def parseLocationName(lib, loc_id):
    """
    Parse location name from library and extract location ID and bbNum.
    
    Parameters:
        lib: The library object with getNameOfLocation method
        loc_id: The location ID from the library
        
    Returns:
        tuple: (loc_id_parsed, loc_name_formatted, bbNum)
            - loc_id_parsed: The parsed numeric location ID
            - loc_name_formatted: The formatted location name (e.g., "L14011_bbNum9592")
            - bbNum: The basic block number (empty string if not present)
    """
    loc_name = lib.getNameOfLocation(loc_id).decode('utf-8')
    Lx = loc_name.split(" + ")[0]
    loc_id_parsed = int(Lx.split("L")[1])
    bbNum = loc_name.split(" + ")[1] if " + " in loc_name else ""
    loc_name_formatted = Lx + "_" + bbNum if bbNum else Lx
    return loc_id_parsed, loc_name_formatted, bbNum

def makeTemplateForFunction (fname,lib):
    global verificationConfig
    
    lib.getInitLocationID.restype = c_int
    initLocID = lib.getInitLocationID ()
    locs = {}
    edges = []
    id_to_bbNum = {}
    visited = set()  # Record the visited locations
    init_loc = None  # The initial location
    locs_with_all_edges_no_instrs = set()  # Track locations where ALL outgoing edges have no instructions

    exit_loc = pyuppaal.Location(name="exit", invariant="", committed=False, id=999999999)
    locs[999999999] = exit_loc

    largest_id = 0

    # Breadth-first traversal queue
    queue = deque([initLocID])
    
    lib.getNameOfLocation.restype = c_char_p
    lib.hasGuard.restype = c_bool
    lib.getEdgeType.restype = c_int # Return the edge type: 0 = SEQUENTIAL, 1 = BRANCH_TRUE, 2 = BRANCH_FALSE, 3 = RETURN, 4 = CALL
    lib.getSuccLocID.restype = c_int
    lib.getInstructionCount.restype = c_int
    
    # Breadth-first traversal modeling
    while queue:
        curr_loc_id = queue.popleft()
        if largest_id < curr_loc_id:
            largest_id = curr_loc_id
        # If the location has been visited, skip
        if curr_loc_id in visited:
            continue
        
        visited.add(curr_loc_id)
        # Create the uppaal location for the current location
        invariant = ""
        committed = False
        loc_id, loc_name, bbNum = parseLocationName(lib, curr_loc_id)
        
        # Get number of outgoing edges first
        nb_edges = lib.nbOfOutgoingEdges(curr_loc_id)
        
        # For non-time properties: all locations are committed
        if timeBounds is None:
            committed = True
        else:
            # For time properties: set committed and time constraints based on priority
            # Priority 1: Use configured time bounds
            # Priority 2: Use estimated time bounds (instruction count)
            # Priority 3: If no instructions, mark as committed
            
            # Calculate max instruction count for this location
            max_instr_count = 0
            if nb_edges > 0:
                for e in range(nb_edges):
                    instr_count = lib.getInstructionCount(curr_loc_id, e)
                    if instr_count > max_instr_count:
                        max_instr_count = instr_count
            
            # Try to get configured time bounds
            has_time_constraint = False
            if bbNum:
                time_bound = getTimeBoundByBBNum(bbNum)
                if time_bound is not None:
                    lower, upper = time_bound
                    invariant = f"x <= {upper}"
                    has_time_constraint = True
                    committed = False
            
            # If no configured time bounds, use instruction count estimation
            if not has_time_constraint:
                if max_instr_count > 0:
                    # Use estimated time bounds
                    upper = max_instr_count * 25
                    invariant = f"x <= {upper}"
                    committed = False
                else:
                    # No instructions: mark as committed
                    invariant = ""
                    committed = True
                    locs_with_all_edges_no_instrs.add(loc_id)
        
        loc = pyuppaal.Location(name=loc_name, invariant=invariant, committed=committed, id=loc_id)
        locs[loc_id] = loc
        id_to_bbNum[loc_id] = bbNum
        
        # Record the initial location
        if curr_loc_id == initLocID:
            init_loc = loc
        
        # Process all outgoing edges of the current location

        if nb_edges == 0:
            # Add edge to exit location
            edges.append(pyuppaal.Transition(
                source=loc,
                target=exit_loc,
                assignment="counter = counter + 1, x = 0",
                guard="",
                synchronisation="",
                comment=f"loc = {curr_loc_id}, exit edge"
            ))

        for e in range(nb_edges):
            g = ""  # uppaal guard
            update = ""  # uppaal update
            
            # Get instruction count for time guard calculation
            instr_count = lib.getInstructionCount(curr_loc_id, e)

            if lib.hasGuard(curr_loc_id, e):
                # succLocID = lib.getSuccLocID(curr_loc_id, e)
                update += "executeEdge ({0}, {1}), g=evaluateGuard ({0}, {1}), counter = counter + 1".format(curr_loc_id, e)
            else:
                update += "executeEdge ({0}, {1}), counter = counter + 1".format(curr_loc_id, e)
                if lib.getEdgeType(curr_loc_id, e) == 1:
                    g = "g"
                elif lib.getEdgeType(curr_loc_id, e) == 2:
                    g = "!g"
            
            # Automatically add verification variable updates from config
            if verificationConfig and 'updates' in verificationConfig:
                for update_expr in verificationConfig['updates']:
                    update += f", {update_expr}"

            # Get the successor location ID
            succ_loc_id = lib.getSuccLocID(curr_loc_id, e)
            
            # Parse the name and ID of the successor location
            succ_loc_id_parsed, succ_loc_name_final, succ_bbNum = parseLocationName(lib, succ_loc_id)
            
            # If the successor location has not been created, create a placeholder
            # The committed status will be determined when we visit that location
            if succ_loc_id_parsed not in locs:
                succ_loc = pyuppaal.Location(name=succ_loc_name_final, invariant="", committed=False, id=succ_loc_id_parsed)
                locs[succ_loc_id_parsed] = succ_loc
                id_to_bbNum[succ_loc_id_parsed] = succ_bbNum
            
            # If the successor location has not been visited, add it to the queue
            if succ_loc_id not in visited:
                queue.append(succ_loc_id)
            
            # Add time guard (lower bound) only for time properties
            time_guard = ""
            if timeBounds is not None:
                # Priority 1: Use configured time bounds
                has_time_guard = False
                if id_to_bbNum[loc_id]:
                    time_bound = getTimeBoundByBBNum(id_to_bbNum[loc_id])
                    if time_bound is not None:
                        lower, upper = time_bound
                        time_guard = f"x >= {lower}"
                        has_time_guard = True
                
                # Priority 2: Use estimated time bounds (instruction count)
                if not has_time_guard and instr_count > 0:
                    lower = instr_count * 1
                    time_guard = f"x >= {lower}"
            
            # Combine time guard with existing guard
            if time_guard:
                if g:
                    g = f"{g} && {time_guard}"
                else:
                    g = time_guard
            
            # Reset clock x in the update (all edges reset the clock)
            update += ", x = 0"
            
            # Create the edge
            if (id_to_bbNum[loc_id] != ""):
                comment = "loc = {0}, edge = {1}, bbNum = {2}".format(curr_loc_id, e, id_to_bbNum[loc_id])
            else:
                comment = "loc = {0}, edge = {1}".format(curr_loc_id, e)
            edges.append(pyuppaal.Transition(
                source=loc,
                target=locs[succ_loc_id_parsed],
                assignment=update,
                guard=g,
                synchronisation="",
                comment=comment
            ))

    # return the template
    # use the name of the function as the name of the template
    # Convert dictionary to list of Location objects for Template
    return pyuppaal.Template (fname, locations = list(locs.values()), initlocation = init_loc, transitions = edges, declaration = "")

'''
    input: libpath——uppllvm lib path
           output——target uppaal model file path
'''
def run (lib_path, output, timeConstraints = None):
    global timeBounds, verificationConfig
    # check output path is suffixed with .xml
    if not output.endswith(".xml"):
        raise ValueError(f"Invalid output file {output}")
    start_time = time.time ()
    lib = cdll.LoadLibrary (lib_path)

    lib.buildCFAModels.restype = None
    lib.buildCFAModels()

    # Parse the IR config
    config = parse_IR_config()
    templates = []
    fname = config.entrypoints[0] # get the name of the entry function
    # create template for the entry function
    timeBounds = timeConstraints if timeConstraints is not None else None
    
    # Parse verification config for the current property
    verificationConfig = parse_verification_config(config.property)
    
    templates.append (makeTemplateForFunction (fname, lib))
    # templates.append (checking)
        
    # for t in templates:
    #     t.assign_ids () # assign ids
        # t.layout () # layout the template
    
    # uppaal golbal declaration
    # Build verification variables declarations from config
    # Support both simple variable names and initialization syntax:
    #   - Simple: 'circle_acceleration_leq_2' -> 'bool circle_acceleration_leq_2;'
    #   - With init: 'circle_acceleration_leq_2 = true' -> 'bool circle_acceleration_leq_2 = true;'
    verification_vars = ""
    if verificationConfig and 'vars' in verificationConfig:
        for var in verificationConfig['vars']:
            verification_vars += f"    bool {var};\n"
    
    # Build clock variables declarations from config
    verification_clocks = ""
    if verificationConfig and 'clocks' in verificationConfig:
        for clock in verificationConfig['clocks']:
            verification_clocks += f"    clock {clock};\n"
    
    decl = '''    
    chan e;
    int g;
    clock x;
    int counter = 0;

{0}
{1}
    import "{2}" {{
    void executeEdge (int l, int e);
    void buildCFAModels ();
    void initModel ();
    int evaluateGuard (int l, int e);

    bool is_true (const string& pName);
    bool is_false (const string& pName);
    bool bit_is_true (const string& pName, int bitIndex);
    bool bit_is_false (const string& pName, int bitIndex);
    bool is_equal_const (const string& pName, double constVal);
    bool is_not_equal_const (const string& pName, double constVal);
    bool is_greater_const (const string& pName, double constVal);
    bool is_greater_or_equal_const (const string& pName, double constVal);
    bool is_less_const (const string& pName, double constVal);
    bool is_less_or_equal_const (const string& pName, double constVal);
    bool is_equal (const string& pName1, const string& pName2);
    bool is_not_equal (const string& pName1, const string& pName2);
    bool is_greater (const string& pName1, const string& pName2);
    bool is_greater_or_equal (const string& pName1, const string& pName2);
    bool is_less (const string& pName1, const string& pName2);
    bool is_less_or_equal (const string& pName1, const string& pName2);
    }};

    void __ON_CONSTRUCT__()
    {{
        buildCFAModels(); 
        initModel();
    }}   
    '''.format (verification_vars, verification_clocks, lib_path)    

    # Create QueryFile object for UPPAAL queries
    queries = pyuppaal.QueryFile()
    if verificationConfig and 'uppaal_formula' in verificationConfig:
        uppaal_formula = verificationConfig['uppaal_formula']
        
        # Get the real initial location name
        lib.getInitLocationID.restype = c_int
        init_loc_id = lib.getInitLocationID()
        _, init_loc_name, _ = parseLocationName(lib, init_loc_id)
        
        # Replace symbolic location names with actual template.location names
        if uppaal_formula and 'locations' in verificationConfig:
            for loc_mapping in verificationConfig['locations']:
                # Parse "symbolic_name = actual_location"
                if '=' in loc_mapping:
                    symbolic_name, actual_location = loc_mapping.split('=', 1)
                    symbolic_name = symbolic_name.strip()
                    actual_location = actual_location.strip()
                    
                    # Special handling: if actual_location is 'init', replace with real initial location
                    if actual_location == 'init':
                        actual_location = init_loc_name
                    
                    # Replace with template_name.location_name format
                    qualified_location = f"{fname.replace('-', '_')}.{actual_location}"
                    uppaal_formula = uppaal_formula.replace(symbolic_name, qualified_location)
        
        if uppaal_formula:
            queries.addQuery(uppaal_formula, f"Property {config.property}")

    # create uppaal model xml file
    nta = pyuppaal.NTA (templates = templates, declaration = decl, system = "system {0};".format (fname).replace ("-","_"), queries = queries)
    
    # Print overall statistics
    total_locations = sum(len(template.locations) for template in templates)
    total_transitions = sum(len(template.transitions) for template in templates)
    print(f"[INFO] Total locations: {total_locations}")
    print(f"[INFO] Total transitions: {total_transitions}")
    
    print("[INFO] writing uppaal model file...")
    with open(output,'w') as writeto:
        writeto.write(nta.to_xml ())
    end_time = time.time()
    execution_time = end_time - start_time
    print(f"[INFO] create uppaal model file time: {execution_time:.2f} s.")


# if __name__ == "__main__":
#     assert(len(sys.argv) == 3)
#     lib_path = sys.argv[1]
#     out = sys.argv[2]
#     #out = sys.argv[1]
#     run (lib_path,out,"A_RTL_P4")

