import threading
import socket
import sys
import ast
from datetime import datetime
import time
import hashlib
from random import shuffle
import struct

# if len(sys.argv) != 3:
#     print("ERROR: Usage: python3 Client.py <IP> <port>")
#     exit(1)
IPAddr = "0.0.0.0"
port = int(10011)
seed_nodes = [('192.168.1.108', 10000)]#('10.196.20.144', 10000)]#'10.129.131.135'] #we can have more than 1 seed 
socket_list = []
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
            seed_nodes = [('192.168.1.108', 10000),('192.168.1.108', 10001)]
            print("new server appended")

def broadcast_messages(mode):
    global seed_nodes, socket_list, port, IPAddr, msg_list
    timer=0
    # list to store the peer list obtained from seed nodes
    p_list = []
    print("In Broadcast...")
    connected_peers=0
    connected_seeds=0
    shuffle(seed_nodes)
    print("shuffled seed nodes list:", seed_nodes)
    while True:
        print("1 Start of while")
        k=0
        connected_seeds=0
        for i in range(len(seed_nodes)):
            node = seed_nodes[i][0]
            node_port = seed_nodes[i][1]
            print("3 seed node:", node)
            try: #get other peers info from seed node(s)
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.connect((node, node_port))
                print("4 Connected to seed node:", node)
                connected_seeds=connected_seeds+1
                if(mode=="low"):
                    values = (b'lowm', port)
                else:
                    values = (b'high', port)
                packer = struct.Struct('4s I')
                packed_data = packer.pack(*values)
                s.sendall(packed_data)
                data = s.recv(1024)
                data = data.decode('utf-8')
                print("5 Got data from seed node %s:%s " % (node, data))
                data = ast.literal_eval(data)
                p_list.extend(data)
                p_list = list(set(p_list))
                print("6 Peer list after connecting to seed node: "+node+" : "+str(p_list))
                s.close()
                if(mode=="low"):
                    timer=1
                else:
                    timer =0.25
                time.sleep(timer)
            except:
                print("7 Error while connection to seed node or fetching Peer list:", sys.exc_info()[1])
                k+=1
                s.close()
        if (k == len(seed_nodes)):
            print("8 No seed is online.")
        if len(p_list) == 0 or (len(p_list)==1 and p_list[0][0]==IPAddr and p_list[0][1]==port):
            print("9 No peers got.")
        # else:
        #     break
        print("10 Pinging after 1sec...")
       

    for peer in seed_nodes:
        if peer[0] == IPAddr and peer[1] == port :
            continue
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if connected_peers == 2:
                break
            sock.connect((peer[0], peer[1]))
            print("Connected to peer: ", peer)
            values = (b'peer', port)
            packer = struct.Struct('4s I')
            packed_data = packer.pack(*values)
            sock.sendall(packed_data)
            connected_peers = connected_peers + 1
            lock.acquire()
            socket_list.append(sock)
            lock.release()
        except:
            print("Error in connecting to peer:", sys.exc_info()[1])

    if connected_peers != 0:
        msg_limit = 10
        while msg_limit != 0:
            t = str(datetime.now())
            msg = t + ": " + IPAddr + ":" + str(port) + " - " + "Hello, This is message number: " +str(msg_limit)
            sha_1 = hashlib.sha1()
            sha_1.update(msg.encode('utf-8'))
            lock.acquire()
            msg_list.append(sha_1.hexdigest())
            lock.release()
            print("Sending... "+msg)
            msg_limit -= 1
            for sock in socket_list:
                sock.sendall(str.encode(msg))
            time.sleep(5)

t1 = threading.Thread(target=accept_connections)
t1.start()

t2 = threading.Thread(target=broadcast_messages,args=("low",))
t2.start()

t1.join()
t2.join()
