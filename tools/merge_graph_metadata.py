#!/usr/env python3

raise RuntimeError("Not debugged or tested yet.")

import argparse
import logging

parser = argparse.ArgumentParser(description='Merge metadata patches into a graph.')
parser.add_argument('base', type=str, help='base graph instance')
parser.add_argument('patches', type=str, nargs='*', action='append', help='Patch files to merge in')
parser.add_argument('-o', type=str, default="-", help="output path")

args = parser.parse_args()

if parser.base=='-':
    src=sys.stdin
    srcPath="<stdin>"
else:
    src=parser.base
    srcPath=parser.base

logging.info("Reading input from {}", srcPath))
graph=load_graph(src,srcPath)

for p in parser.patches:
    logging.info("Loading patch file {}", p)
    patch=load_graph_metadata(p)
    logging.info("Applying patch patch")
    patch.transfer_to_graph(g)

if parser.o=="-":
    dst=sys.stdout
else:
    logging.info("Opening output file {}", parser.o)
    dst=open(parser.o,"wt")

logging.info("Writing ouput"))
save_graph(graph, dst)

