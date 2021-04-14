import threading
import socket
import sys
import ast
from datetime import datetime
import time
import hashlib
from random import shuffle
import struct

if len(sys.argv) != 2:
    print("ERROR: Usage: python3 Client.py <Mode of operation(high/low)>")
    exit(1)
IPAddr = "0.0.0.0"
port = int(10011)
socket_list=[]
#seed_nodes = [('192.168.1.108', 10001)]
seed_nodes = [('10.0.2.1', 10001)]
#seed_nodes =[("192.168.122.186",10002)]
msg_list = []
lock = threading.Lock()


print("Welcome: ", (IPAddr+str(port)))

def listen_to_connections(conn, addr, conn_port):
    global msg_list, socket_list, file, lock
    print("Listening to: ", addr)
    while True:
        data = conn.recv(1024) #no need to encode data here
        data = data.decode('utf-8')
        sha_1 = hashlib.sha1()
        sha_1.update(data.encode('utf-8'))
        lock.acquire()
        if sha_1.hexdigest() not in msg_list: #See if the client already has msg, if not add it to msg_list
            msg_list.append(sha_1.hexdigest())
            file = open("outputs.txt", "a+")
            file.write(data + " : RECEIVED FROM " + addr[0] + ":" + str(conn_port) + ", RECEIVED AT " + IPAddr + ":" + str(port) +"\n")
            file.close()
            for sock in socket_list:
                sock.sendall(str.encode(data))
        lock.release()


def accept_connections():
    global port, IPAddr,seed_nodes
    print("Accepting connections...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind((IPAddr, port))
    sock.listen(5)
    while True:
        conn, addr = sock.accept()
        print("In accept: got connection request from "+str(addr))
        unpacker = struct.Struct('4s I')
        data = conn.recv(unpacker.size)
        data = unpacker.unpack(data)
        print("In accept, connected to: " + addr[0] + ":" + str(addr[1]) + " Got data: " + str(data))
        
        if data[0] == b"peer":
            t = threading.Thread(target=listen_to_connections, args=(conn,addr,data[1]))
            t.start()
	
        if data[0] == b"news":
            seed_nodes = [('192.168.122.79', 10001),("192.168.122.186",10002)]
            print("New Server added due to load increase")

def broadcast_messages(mode):
    global seed_nodes, socket_list, port, IPAddr, msg_list
    timer=0
    # list to store the peer list obtained from seed nodes
    p_list = []
    print("In Broadcast...")
    while True:
        k=0
        connected_seeds=0
        for i in range(len(seed_nodes)):
            node = seed_nodes[i][0]
            node_port = seed_nodes[i][1]
            print("3 seed node:", node)
            try: #get other peers info from seed node(s)
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.connect((node, node_port))
                print("Connected to seed node:", node)
                connected_seeds=connected_seeds+1
                values = (b'peer', port)
                if(mode=="low"):                    
                    timer=1
                else:
                    timer =0.25
                packer = struct.Struct('4s I')
                packed_data = packer.pack(*values)
                s.sendall(packed_data)
                data = s.recv(1024)
                data = data.decode('utf-8')
                print("Got data from seed node %s:%s " % (node, data))
                s.close()
                time.sleep(timer)
            except:
                print("7 Error while connection to seed node or fetching Peer list:", sys.exc_info()[1])
                k+=1
                s.close()
        if (k == len(seed_nodes)):
            print("No server is online!")
       

t1 = threading.Thread(target=accept_connections)
t1.start()

t2 = threading.Thread(target=broadcast_messages,args=(sys.argv[1],))
t2.start()

t1.join()
t2.join()
