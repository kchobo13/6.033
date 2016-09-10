import sys
import os

def run(query, cookie):
    body = open("grades.txt").read()
    return "<h1>Grades</h1>\n<pre>\n%s</pre>\n" % (body,)

