# ----------------------------------------------------------------------------
#
#   Name:  Alex Meyer (meyerale)
#   Clase:  CS372, F17
#   Assignment:  program 2
#   Due Date:  10/26/2017
#   Description:  File transfer client that connects to a server and requests
#         either a list of files or a file, and listens on a given port for
#         the server to send on the data connection.
#
#       Socket setup code is based on program 4 from CS344 with some modifications to
#       meet the requirements of this assignment.
#
# ----------------------------------------------------------------------------

from socket import *
import threading
import sys
import os
import struct


# =============================================================================
# If the incorrect number or type of options are used, then report correct usage
# =============================================================================
def exitError():
    print("USAGE:  ftclient.py server_host control_port [-l | -g filename] data_port")
    exit(0)


# =============================================================================
# Contrust the request that will be sent to the server from the arguments
# entered by the user.
# =============================================================================
def constructRequest():
    request = []

    if (sys.argv[3] == "-l"):
        request.append(sys.argv[3])  # option 'l'
        request.append(sys.argv[4])  # port number
    elif (sys.argv[3] == "-g" and len(sys.argv) == 6):
        request.append(sys.argv[3])  # option 'g'
        request.append(sys.argv[4])  # file name
        request.append(sys.argv[5])  # port number
    else:
        exitError()

    # create a single request string delimited by a space
    requestString = " ".join(request)
    return requestString

# =============================================================================
# Create the control connection with the server and send the command request
# =============================================================================
def makeControlConnection(msg):
    serverHost = sys.argv[1]
    controlPort = int(sys.argv[2])

    #create socket and send request
    clientControlSocket = socket(AF_INET, SOCK_STREAM)
    clientControlSocket.connect((serverHost, controlPort))
    clientControlSocket.send(msg.encode())

    #get response
    response = clientControlSocket.recv(256).decode()
    if (response.split('&', 1)[0] == "404"):
        print (response.split('&', 1)[1])

    #close control socket
    clientControlSocket.close()

# =============================================================================
# Setup a listening socket for data transfer from the server
# =============================================================================
def makeDataConnection():
    clientDataHost = gethostname()
    # clientDataHost = 'localhost'
    clientDataPort = int(sys.argv[-1])

    #setup socket and listen for connection
    clientDataSocket = socket(AF_INET, SOCK_STREAM)
    clientDataSocket.settimeout(5)
    clientDataSocket.bind((clientDataHost, clientDataPort))
    clientDataSocket.listen(5)

    try:
        connectionDataSocket, addr = clientDataSocket.accept()

        # read the file size from the first 4 bytes of the response
        fileSize = struct.unpack(">I", connectionDataSocket.recv(4))[0]

        # build the rest of the response
        msg = ""
        while (len(msg) < fileSize):
            msg += connectionDataSocket.recv(256).decode()

        if (msg == "FILE NOT FOUND"):
            raise FileNotFoundError

        if (sys.argv[3] == "-g"):      # file was requested
            baseName = sys.argv[4]
            fileNumber = 1

            #append or increment a number at end of file name to handle duplicate file names
            while (os.path.isfile(baseName) == True):
                i = baseName.find(".")
                fileNumber += 1
                if (baseName[i-1].isdigit()):
                    baseName = baseName[:i-1] + baseName[i:]
                    i -= 1
                baseName = baseName[:i] + str(fileNumber) + baseName[i:]

            if (fileNumber > 1):
                print("File already exists.  Saving as", baseName)
            f = open(baseName, "w+")
            f.write(msg)
            f.close()
        else:               # file list was requested
            print(msg)

    except timeout:     # socket timed out
        pass

    except FileNotFoundError:       # file requested could not be found
        print(gethostbyaddr(addr[0])[0], ":", sys.argv[-1], "says", msg)


# =============================================================================
if not (4 < len(sys.argv) < 7):
    exitError()

msg = constructRequest()


# Setup threads
# https://pymotw.com/2/threading/
controlThread = threading.Thread(target=makeControlConnection, args=(msg, ))
dataThread = threading.Thread(target=makeDataConnection)

# Start threads
controlThread.start()
dataThread.start()

