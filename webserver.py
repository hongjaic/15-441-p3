#!/usr/bin/env python

import os
import hashlib
import socket

app = Flask(__name__)
	
@app.route('/')
def index():
   #1. Connect to the routing daemon on port p
    #2. Do GETRD <object-name> 
    #3. Parse the response from the routing daemon
    #3 a) If response is OK <URL>, the open the URL
    #3 b) If response is 404, then show that content is  not available
    #### You may factor out things from here and rd_getrd() function and form a separate sub-routine
	return "Unimplemented"



if __name__ == '__main__':
	if (len(sys.argv) > 1):
		servport = int(sys.argv[1])
		app.run(host='0.0.0.0', port=servport, threaded=True, processes=1)
	else:	
		print "Usage ./webserver <server-port> \n"
