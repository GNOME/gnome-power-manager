import sys

def error(*lines):
    for line in lines:
        print "error: " + str(line)
    sys.exit(1)

def warning(*lines):
    for line in lines:
        print "warning: " + str(line)

