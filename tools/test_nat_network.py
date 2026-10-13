"""Verify real traffic through the specified Wi-Fi client IPv4 address."""
import argparse
import json
from pathlib import Path
import secrets
import socket
import ssl
import struct
import time

parser=argparse.ArgumentParser()
parser.add_argument('--client',required=True)
parser.add_argument('--dns',default='192.168.0.1')
parser.add_argument('--log',required=True)
args=parser.parse_args()
results={'client':args.client,'tests':[]}

def record(name,fn):
    start=time.monotonic()
    try:
        details=fn()
        row={'name':name,'passed':True,'seconds':round(time.monotonic()-start,3),'details':details}
    except Exception as error:
        row={'name':name,'passed':False,'seconds':round(time.monotonic()-start,3),'error':str(error)}
    results['tests'].append(row)
    print(json.dumps(row),flush=True)

def dns(server,name='example.com'):
    ident=secrets.randbelow(65536)
    qname=b''.join(bytes([len(part)])+part.encode('ascii') for part in name.split('.'))+b'\0'
    query=struct.pack('!6H',ident,0x0100,1,0,0,0)+qname+struct.pack('!HH',1,1)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as s:
        s.bind((args.client,0));s.settimeout(10)
        s.sendto(query,(server,53));reply,peer=s.recvfrom(2048)
    rid,flags,questions,answers,_,_=struct.unpack('!6H',reply[:12])
    if rid!=ident or flags&15 or not answers or peer[0]!=server:
        raise RuntimeError('Invalid DNS response')
    def skip_name(pos):
        while reply[pos]:
            if reply[pos]&0xc0==0xc0:return pos+2
            pos+=1+reply[pos]
        return pos+1
    pos=12
    for _ in range(questions):pos=skip_name(pos)+4
    addresses=[]
    for _ in range(answers):
        pos=skip_name(pos)
        kind,clazz,ttl,length=struct.unpack('!HHIH',reply[pos:pos+10]);pos+=10
        if kind==1 and clazz==1 and length==4:addresses.append(socket.inet_ntoa(reply[pos:pos+4]))
        pos+=length
    return {'server':server,'answers':answers,'replyBytes':len(reply),'addresses':addresses}

def http(tls=False):
    hostname='www.baidu.com' if tls else 'one.one.one.one'
    remote=dns(args.dns,hostname)['addresses'][0] if tls else '1.1.1.1'
    with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as s:
        s.bind((args.client,0));s.settimeout(12);s.connect((remote,443 if tls else 80))
        stream=ssl.create_default_context().wrap_socket(s,server_hostname=hostname) if tls else s
        try:
            stream.sendall(('GET / HTTP/1.1\r\nHost: '+hostname+'\r\nConnection: close\r\n\r\n').encode('ascii'))
            reply=stream.recv(4096)
            status=reply.split(b'\r\n',1)[0].decode('ascii')
            if not status.startswith('HTTP/') or len(status.split())<2:raise RuntimeError('No HTTP response')
            return {'remote':remote,'host':hostname,'port':443 if tls else 80,'status':status,'tls':stream.version() if tls else None}
        finally:
            if tls:stream.close()

record('UDP DNS via upstream router',lambda:dns(args.dns))
record('UDP DNS via public server',lambda:dns('1.1.1.1'))
record('Outbound TCP HTTP',lambda:http(False))
record('Outbound TCP HTTPS',lambda:http(True))
results['passed']=all(r['passed'] for r in results['tests'])
path=Path(args.log);path.parent.mkdir(parents=True,exist_ok=True)
path.write_text(json.dumps(results,indent=2),encoding='utf-8')
raise SystemExit(0 if results['passed'] else 1)
