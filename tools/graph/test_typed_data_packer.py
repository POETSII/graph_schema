from graph.core import *
from typed_data_packer import make_typed_data_packer

import unittest

class TestScalarPacking(unittest.TestCase):
    def test_int8_zero(self):
        s=ScalarTypedDataSpec("_", "int8_t")
        p=make_typed_data_packer(s)
        bv=p.unpack(bytes([6]))
        self.assertEqual(bv, 6)
        bv=p.unpack(bytes([255]))
        self.assertEqual(bv, -1)

    def test_uint8_zero(self):
        s=ScalarTypedDataSpec("_", "uint8_t")
        p=make_typed_data_packer(s)
        bv=p.unpack(bytes([6]))
        self.assertEqual(bv, 6)
        bv=p.unpack(bytes([255]))
        self.assertEqual(bv, 255)
        self.assertEqual(p.byte_size,1)

    def test_float_zero(self):
        s=ScalarTypedDataSpec("_", "float")
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size,4)

    def test_float_three(self):
        s=ScalarTypedDataSpec("_", "float", 3.0)
        p=make_typed_data_packer(s)

class TestTuplePacking(unittest.TestCase):

    def test_empty(self):
        s=TupleTypedDataSpec("_", [])
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size,0)

    def test_scalar(self):
        s=TupleTypedDataSpec("_", [ScalarTypedDataSpec("x","int8_t")])
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size,1)

    def test_scalar_3(self):
        s=TupleTypedDataSpec("_", [ScalarTypedDataSpec("x","int8_t",3)])
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size,1)
        self.assertEqual(p.default_bytes, bytes([3]))

    def test_scalar_4_float_5(self):
        s=TupleTypedDataSpec("_", [ScalarTypedDataSpec("x","int8_t",4),ScalarTypedDataSpec("y","float",5)])
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size,5)
        self.assertEqual(p.unpack(p.default_bytes),{"x":4, "y":5})

class TestArrayPacking(unittest.TestCase):

    def test_empty(self):
        try:
            s=ArrayTypedDataSpec("_", 0, ScalarTypedDataSpec("_", "int8_t"))
            self.assertFalse()
        except:
            pass

    def test_one(self):
        s=ArrayTypedDataSpec("_", 1, ScalarTypedDataSpec("_", "uint16_t"))
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size, 2)

    def test_two(self):
        s=ArrayTypedDataSpec("_", 2, ScalarTypedDataSpec("_", "uint16_t"))
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size, 4)

    def test_tuple(self):
        s=ArrayTypedDataSpec("_", 9, TupleTypedDataSpec("_", [ScalarTypedDataSpec("a","uint16_t"),ScalarTypedDataSpec("b","uint8_t")]))
        p=make_typed_data_packer(s)
        self.assertEqual(p.byte_size, 9*3)
    
if __name__ == '__main__':
    unittest.main()