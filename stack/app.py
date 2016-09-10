#!/usr/bin/python
import os
import sys
import cgi
import Cookie
import applogic
import traceback

## Parse the query string, cookie
query = cgi.parse()
cookie = Cookie.SimpleCookie()
if not (os.getenv("HTTP_COOKIE") is None):
    cookie.load(os.getenv("HTTP_COOKIE"))

try:
    body = applogic.run(query, cookie)
except:
    body = "<H1>Exception</H1>\n" + \
           "<PRE>\n" + \
           traceback.format_exc() + \
           "</PRE>\n";

## Use sys.stdout.write to ensure correct CRLF endings
sys.stdout.write("HTTP/1.0 200 OK\r\n")
cookiestr = cookie.output()
if cookiestr != "":
    sys.stdout.write(cookiestr + "\r\n")

sys.stdout.write("Content-Type: text/html\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)

