import coverage
import os
import sys
import unittest

args = sys.argv[:]
args.pop(0) # Remove program name.

configuration = ""
if len(args):
  configuration = args.pop(0)

arg1 = ""
if len(args):
  arg1 = args.pop(0)

suite = unittest.TestLoader().discover(".")

cov = coverage.Coverage(source=["libcellml"])
cov.set_option("run:plugins", ["exclude_until_coverage_plugin"])
cov.set_option("exclude_until_coverage_plugin:marker", "# libCellML generated wrapper code starts here.")

os.chdir(os.path.join("@PYTHON_COVERAGE_WORKING_DIRECTORY@", configuration))
cov.start()

unittest.TextTestRunner(verbosity=1).run(suite)

cov.stop()
cov.save()

if arg1 == "html":
  cov.html_report(directory='coverage_html_report')
else:
  cov.report()
