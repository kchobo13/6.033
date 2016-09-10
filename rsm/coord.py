#!/usr/bin/python

import sys, socket

replicas = map(int, sys.argv[1:])

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(('', 0))
s.listen(4)
print "\nListening on port " + str(s.getsockname()[1]) + " ..."

while True:
  conn, addr = s.accept()
  msg = conn.recv(4096)
  replies = []

  # Below goes your code. You need to write between 5-10 lines of code.
  # If you are writing more, you are doing something wrong.
  # the variables replies and replicas should be used.

  date = strftime("#, %a, %d %b %Y %H:%M:%S +0000\n", gmtime())

  for replica in replicas:
    connection = socket.create_connection(('localhost', replica))
    connection.send("%s:%s" % (msg, time))

    response = connection.recv(4096)
    replies.append(response)

  print replies
  conn.send(replies[0])
  conn.close()
