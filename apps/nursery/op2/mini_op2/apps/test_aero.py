#!/usr/bin/env python3

import logging
import unittest
import os
import io

from mini_op2.apps.aero import *
import mini_op2.framework.sync_compiler

import unittest

class TestAeroExecute(unittest.TestCase):
    def setUp(self):
        self.here=os.path.dirname(os.path.realpath(__file__))
        self.srcFile="{}/../../meshes/aero_1.5625%.hdf5".format(self.here)
        self.srcFile=os.path.abspath(self.srcFile)
       
    def test_execute_system(self):
        (spec,inst,code)=build_system(self.srcFile, total_iter=4)
        code.execute(inst)
        
    def test_compile_provider(self):
        (spec,inst,code)=build_system(self.srcFile)
        builder=mini_op2.framework.sync_compiler.sync_compiler(spec,code)
        xml=builder.build_and_render()

    

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    unittest.main()
