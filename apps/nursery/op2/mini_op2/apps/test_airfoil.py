#!/usr/bin/env python3

import logging
import unittest
import os

from mini_op2.apps.airfoil import *

import unittest

class TestAirfoilExecute(unittest.TestCase):
    
    def test_execute_system(self):
        here=os.path.dirname(os.path.realpath(__file__))
        print(here)
        srcFile="{}/../../meshes/airfoil_1.5625%.hdf5".format(here)
        srcFile=os.path.abspath(srcFile)
        print(srcFile)
        (spec,inst,code)=build_system(srcFile, maxiter=4)
        code.execute(inst)

if __name__=="__main__":
    logging.basicConfig(level=4)
    
    unittest.main()
