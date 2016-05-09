import socket
import argparse
import threading

DEFAULT_PORT = 1234
#https://docs.python.org/2/howto/sockets.html

parser = argparse.ArgumentParser(description='UDP tool')
parser.add_argument('--port', type=int, default=DEFAULT_PORT,
                    help="Port to send to")
parser.add_argument('--addr', type=str, required=True,
                    help="Addr to send to E.X. "
                    "'fd00:ff1:ce0b:a5e0:fec2:3d00:4:ea8c'")
args = parser.parse_args()

addr = args.addr
port = args.port

done = False

def read_socket(socket):
    while (True):
        data = socket.recv(4096)
        if len(data) > 0:
            print("Data recieved: %s" % data)
        if done:
            print("Read thread exiting")
            break
        


#HOST = socket.gethostname()
#info = socket.getaddrinfo(HOST, 1234)
#print("Info: %s" % info)
sock = socket.socket(socket.AF_INET6, # Internet
                     socket.SOCK_STREAM)
#sock.bind(('', 1234))
sock.connect((addr, port))
read_thread = threading.Thread(target=read_socket, args=(sock,))
read_thread.daemon = True
read_thread.start()
try:
    while (True):
        data = raw_input("Enter command:")
        sock.send(data)
        print("Message sent: %s" % data)
finally:
    sock.shutdown(socket.SHUT_RDWR)
    sock.close()
