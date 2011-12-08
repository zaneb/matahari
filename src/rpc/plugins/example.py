
def testFunction():
    return 'Result of the testFunction() function'

class TestClass(object):
    pass

def testException():
    raise Exception('Exception thrown by the testException() function')

def showArgs(*args, **kwargs):
    return {'args': args, 'kwargs': kwargs}

def _hidden():
    raise Exception('This method should not be visible')
