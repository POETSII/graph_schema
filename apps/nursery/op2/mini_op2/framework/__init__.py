import unittest

def load_tests(loader, tests, pattern):
    suite = unittest.TestSuite()
    import mini_op2.framework.user_code_parser
    suite.addTest(loader.loadTestsFromModule(mini_op2.framework.user_code_parser, pattern))
    return suite
