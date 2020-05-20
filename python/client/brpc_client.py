#coding: utf-8

import sys
import socket
import struct
import os
sys.path.append(os.path.abspath(os.path.dirname(__file__)))
from  proto_lib import baidu_rpc_meta_pb2 as baidu_rpc_meta_pb2

TIME_OUT = 1000000

class BrpcClient(object):
    """docstring for BrpcClient"""
    def __init__(self, ip, port):
        self._socket_connection = None
        self._ip = ip
        self._port = port

    def _connect(self):
        try:
            socket_obj = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            socket_obj.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            socket_obj.connect((self._ip, self._port))
            socket_obj.settimeout(TIME_OUT*1.0/1000)
            return True, socket_obj
        except Exception as e:
            return False, str(e)

    def _gen_header(self, body_size, meta_size):
        header = struct.pack("!4sII", "PRPC", body_size, meta_size)
        return [ header ]

    def _gen_meta_header(self, service_name, method_name):
        rpc_meta = baidu_rpc_meta_pb2.RpcMeta()
        request = rpc_meta.request
        request.service_name = service_name
        request.method_name = method_name
        request.log_id = 1234
        request.trace_id = 5678 
        meta_str = rpc_meta.SerializeToString()
        return meta_str 
      
    def send_req(self, service_name, method_name, pb_packet, res_pb):
        return_data = b''
        if self._socket_connection == None:
            connect_succ, ret_obj = self._connect()
            if not connect_succ:
                return False, ret_obj
            else:
                self._socket_connection = ret_obj
        try:
            meta_str = self._gen_meta_header(service_name, method_name)
            s_req_str = pb_packet.SerializeToString()
            meta_size = len(meta_str)
            body_size = meta_size + len(s_req_str)
            packet_bytes = self._gen_header(body_size, meta_size)
            packet_bytes.append(meta_str)
            packet_bytes.append(s_req_str)
            data = b""
            for v in packet_bytes:
              data = data + v 
            self._socket_connection.sendall(data)
            # print("send packet {0}".format(len(data)))
            header_bytes = self._socket_connection.recv(12);
            packet_header = struct.unpack("!4sII", header_bytes)
            size = int(packet_header[1])
            while True:
                return_data += self._socket_connection.recv(size)
                if len(return_data) >= size:
                    break
            raw_meta = return_data[0: packet_header[2]] 
            rpc_meta = baidu_rpc_meta_pb2.RpcMeta()
            rpc_meta.ParseFromString(raw_meta)
            #print(rpc_meta)
            raw_res = return_data[packet_header[2]:]
            res_pb.ParseFromString(raw_res)
            return True
        except Exception as e:
            print("exception " + str(e))
            return False


if __name__ == '__main__':
    import echo_pb2
    ip = "localhost"
    port = 8000
    client = BrpcClient(ip, port)
    req_msg = echo_pb2.EchoRequest()
    req_msg.message = "hello world"
    res_msg = echo_pb2.EchoResponse()
    rc = client.send_req("EchoService", "Echo", req_msg, res_msg)
    if not rc:
        sys.exit(1)
    print("recieve the response sucessfully")
    print("response: {0}".format(res_msg.message))
    

