Alex Meyer (meyerale)
CS372 - F2017
Project 2
Due 11/26/2017


TCP File Transfer Server:

COMPILE:
In the serverFolder directory...
Compile chat client with:  gcc -o ftserve ftserve.c


EXECUTE:
1.  Start file transfer server with:  ./ftserve <portnumber>
	-- portnumber = any open port you would like to use

2.  Start file transfer client with:  
		python3 ./ftclient <hostname> <portnumber> [-l | -g <filename>] <dataport>
	-- hostname = server that ftserve.c is running on
	-- portnumber = port number for ftserve.c process
	-- -l to list the files in the serverFolder
	-- -g to get a specific file
	-- filename = the file to get
	-- dataport = port number for the data connection

