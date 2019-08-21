from graph.core import GraphType, GraphInstance
from typing import *
import io
import lxml.etree
import xml.etree
import warnings

from graph.load_xml_v4 import v4_namespace_uri, v4_load_graph_types_and_instances
from graph.load_xml_v3 import v3_namespace_uri, v3_load_graph_types_and_instances

def load_graph(src : Union[str, io.TextIOBase, lxml.etree.Element], src_path : Optional[str] = None ) -> (GraphType, Optional[GraphInstance]):
    """
    src is a string, stream, or XML Element to load from.
    If it is an Element, it should be the root Graphs element.

    src_path optionally gives the file-name (including extension) of the place where the source file
    came from. This is used for some kinds of error handling, embedded #path info into C source
    files, and may be used for looking for implied meta-data and pre-compiled files. However, it
    should still work even without a path.
    """
    if isinstance(src,(str,io.TextIOBase)):
        tree=lxml.etree.parse(src)
        graph=tree.getroot() # type: lxml.etree.Element
    elif isinstance(src,lxml.etree.ElementBase):
        graph=src # type: lxml.etree.Element
    else:
        raise RuntimeError(f"Didn't know how to load from src of type {type(src)}")

    if graph.tag==f"{{{v4_namespace_uri}}}Graphs":
        return v4_load_graph_types_and_instances(graph, src_path)
    elif graph.tag==f"{{{v3_namespace_uri}}}Graphs":
        return v3_load_graph_types_and_instances(graph, src_path)
    else:
        raise RuntimeError(f"Didn't know how to deal with root element of type {graph.tag}")

def load_graph_type(src : Union[str, io.TextIOBase, lxml.etree.Element], src_path : Optional[str] ) -> GraphType:
    (gt,gi)=load_graph(src, src_path)
    return gt

def load_graph_instance(src : Union[str, io.TextIOBase, lxml.etree.Element], src_path : Optional[str] ) -> GraphInstance:
    (gt,gi)=load_graph(src, src_path)
    if gi==None:
        raise RuntimeError("No GraphInstance in the given source")
    return gi

def load_graph_types_and_instances(src, src_path ):
    warnings.warn("Users should switch to load_graph", category=DeprecationWarning)
    (gt,gi) = load_graph(src, src_path)
    if gi is None:
        return ({gt.id:gt},{})
    else:
        return ({gt.id:gt},{gi.id:gi})
