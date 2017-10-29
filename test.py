import socket

data = \
"GET / HTTP/1.1\r\n" + \
"Host: example.com\r\n" + \
"Upgrade: websocket\r\n" + \
"Connection: upgrade\r\n" + \
"Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" + \
"\r\n"

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect(('localhost', 11011))
s.send(data.encode())

resp = s.recv(8192)
print(resp.decode())



s.close()