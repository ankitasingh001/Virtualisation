import threading
import socket
import sys
import ast
from datetime import datetime
import time
import hashlib
import struct

# if len(sys.argv) != 3:
#     print("ERROR: Usage: python3 Seed.py <Seed IP> <Seed port>")
#     exit(1)
IPAddr = "0.0.0.0"
peer_list = []
socket_list = []
# port = 10001 # Port for accepting connections
port = int(10001)
msg_list = []
LARGE_NUM=5665665665665665665665665665665665665665665665665665665665
lock = threading.Lock()

print("Welcome to seed node : " + IPAddr)

def generate(x):
    i=0
    while(i<1000000):
        k=x*x*x*x*x
        i+=1

def use(mode):
    i=0
    while(i<100):
        generate(5665665665665665665665665665665665665665665665665665665665)
        i+=1



def accept_connections():
    global peer_list, port, IPAddr
    print("Accepting connections from peers...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((IPAddr, port))
    sock.listen(5) #max no of qued connections =5 here
    while True:
        #conn is a new socket object usable to send and receive data on 
        #the connection, and address is the address bound to the socket on the other
        #end of the connection        
        conn, addr = sock.accept()  
        # print("In accept: got connection request from "+str(addr))
        # data = conn.recv(4)  #recieve the data
        # data = data.decode('utf-8')
        unpacker = struct.Struct('4s I')
        data = conn.recv(unpacker.size)
        data = unpacker.unpack(data)
        print("In accept, connected to:", addr[0], ":", data[1], "Got data:", data)
        if data[0] == b'lowm':
            conn.send(str.encode(str(peer_list)))
            addr = (addr[0], data[1])
            peer_list.append(addr)
            peer_list = list(set(peer_list))
            print("Got to connect in low load mode")
            generate(LARGE_NUM)
            conn.close()
        if data[0] == b'high':
            conn.send(str.encode(str(peer_list)))
            addr = (addr[0], data[1])
            peer_list.append(addr)
            peer_list = list(set(peer_list))
            generate(LARGE_NUM)
            print("Got to connect in high load mode")
            conn.close()
			
t1 = threading.Thread(target=accept_connections)
t1.start()
t1.join()
