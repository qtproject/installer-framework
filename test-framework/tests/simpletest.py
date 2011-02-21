import sys
from testrunner import testrunner

class Result:
    def addError( self, errstr ):
        print errstr
    

result = Result()
runner = testrunner.TestRunner( sys.argv[1], result )
runner.run()
