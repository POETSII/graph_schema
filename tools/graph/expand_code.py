import re
import os
import sys
from typing import *

from graph.core import GraphType, DeviceType

_re_EMBED_HEADER_LINE=re.compile(r"\s*[#]pragma\s+POETS\s+EMBED_HEADER\s*", re.RegexFlag.MULTILINE)
_re_INCLUDE_HEADER_LINE=re.compile(r'\s*[#]include\s+"([^"]*)"s*', re.RegexFlag.MULTILINE)

def expand_source_code(src:str, base_path:Optional[str]) -> str:
    """Looks through source code for any instance of #pragma POETS EMBED_HEADER
        If it finds such a pragma, and:
        - The following line is a relative #include (i.e. using speech marks, not angle)
        - The referenced header can be found, either at an absolute path or relative to the input
        Then the contents of the header file will be inserted at that point in the file

        The search process is:
        1 - Try path as an absolute path.
        2 - Try path relative to base_path
        3 - Try path relative to working dir

        It does not play around with #lines, but leaves behind a pair:
        #pragma POETS BEGIN_EMBEDDED_HEADER include_path full_path
        <inserted code>
        #pragma POETS END_EMBEDDED_HEADER include_path full_path

        base_path should be the path to a filename, not to a directory.
    """
    res=[]
    prev_match=None
    for line in src.splitlines(keepends=True):
        if prev_match is None:
            m=_re_EMBED_HEADER_LINE.match(line)
            if m:
                # We hold onto the line here
                prev_match=line
            else:
                res.append(line)
        else:
            m=_re_INCLUDE_HEADER_LINE.match(line)
            if not m:
                sys.stderr.write('Warning: found #pragma POETS EMBED_HEADER, but following line was not valid #include "<path>" ')
                res.append(prev_match)
                res.append(line)
                prev_match=None
            else:
                original_path=m.group(1)
                full_path=None
                full_source=None

                # Search for a file we can open
                if os.path.isfile(original_path):
                    full_path=original_path
                if full_path is None and base_path is not None:
                    full_path=os.path.normpath( os.path.join( os.path.dirname( base_path ), original_path ) )
                    if not os.path.isfile(full_path):
                        full_path=None
                if full_path is None:
                    full_path=os.path.abspath(original_path)
                    if not os.path.isfile(full_path):
                        full_path=None

                try:
                    with open(full_path,"rt") as src:
                        full_source=src.read()
                except:
                    full_source=None
                
                if full_source is None:
                    sys.stderr.write(f'Warning: found #pragma POETS EMBED_HEADER, but either could not find or count not read source file "{original_path}"')
                    res.append(prev_match)
                    res.append(line)
                else:
                    res.append(f"////////////////////////////////////////////////////////\n")
                    res.append(f"//\n")
                    res.append(f'#pragma POETS BEGIN_EMBEDDED_HEADER "{original_path}" "{full_path}"\n')
                    res.append(full_source)
                    res.append("\n")
                    res.append(f'#pragma POETS END_EMBEDDED_HEADER "{original_path}" "{full_path}"\n')
                    res.append(f"//\n")
                    res.append(f"////////////////////////////////////////////////////////\n")
            
                prev_match=None 

    return "".join(res)

def expand_graph_type_source(gt:GraphType, base_path:str) -> None:
    expand = lambda x: expand_source_code(x, base_path)
    expand_array = lambda x: list(map(expand, x))

    gt.shared_code = expand_array(gt.shared_code) 
    for dt in gt.device_types.values():  # type: DeviceType
        #sys.stderr.write(f"Pre-expand {dt.id}: {dt.shared_code}\n")
        dt.shared_code = expand_array(dt.shared_code)
        #sys.stderr.write(f"Post-expand {dt.id}: {dt.shared_code}\n")
        for ip in dt.inputs_by_index:
            ip.receive_handler = expand(ip.receive_handler)
        for op in dt.outputs_by_index:
            op.send_handler = expand(op.send_handler)
        dt.ready_to_send_handler = expand(dt.ready_to_send_handler)
        dt.init_handler = expand(dt.init_handler)
        dt.on_hardware_idle_handler = expand(dt.on_hardware_idle_handler)
        dt.on_device_idle_handler = expand(dt.on_device_idle_handler)
