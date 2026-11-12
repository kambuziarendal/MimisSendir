#!/usr/bin/env python3
"""Synthetic peer + status identity on loopback only. Never speaks to Signal."""
import argparse
import base64
from pathlib import Path
import socket
import ssl
import threading
import time
import xml.etree.ElementTree as ET
from xml.sax.saxutils import escape, quoteattr

class Stream:
    def __init__(self, sock):
        self.sock=sock; self.parser=ET.XMLPullParser(['start','end']);self.depth=0
    def send(self,text):self.sock.sendall(text.encode())
    def next(self):
        while True:
            for kind,elem in self.parser.read_events():
                if kind=='start':
                    self.depth+=1
                    if self.depth==1:self.root=elem
                else:
                    self.depth-=1
                    if self.depth==1:
                        self.root.remove(elem)
                        return elem
            data=self.sock.recv(65536)
            if not data:raise EOFError()
            self.parser.feed(data)
    def open(self):
        self.send("<stream:stream to='fixture.test' xmlns='jabber:client' xmlns:stream='http://etherx.jabber.org/streams' version='1.0'>")
        return self.next()
    def result(self,identifier):
        while True:
            stanza=self.next()
            if stanza.get('id')==identifier:
                assert stanza.get('type')=='result', stanza.get('type')
                return stanza

def login(who,ca,resource='stability-fixture'):
    raw=socket.create_connection(('127.0.0.1',15422),timeout=10)
    stream=Stream(raw);stream.open()
    stream.send("<starttls xmlns='urn:ietf:params:xml:ns:xmpp-tls'/>")
    assert stream.next().tag.endswith('proceed')
    ctx=ssl.create_default_context(cafile=str(ca))
    stream=Stream(ctx.wrap_socket(raw,server_hostname='fixture.test'));stream.open()
    password='fixture-only-'+who
    auth=base64.b64encode(('\0'+who+'\0'+password).encode()).decode()
    stream.send("<auth xmlns='urn:ietf:params:xml:ns:xmpp-sasl' mechanism='PLAIN'>"+auth+'</auth>')
    assert stream.next().tag.endswith('success')
    stream=Stream(stream.sock);stream.open()
    stream.send("<iq type='set' id='bind'><bind xmlns='urn:ietf:params:xml:ns:xmpp-bind'><resource>"+escape(resource)+"</resource></bind></iq>")
    stream.result('bind');return stream


def status_peer(ca,stop):
    stream=login('status',ca)
    stream.sock.settimeout(1)
    last=0
    while not stop.is_set():
        if time.monotonic()-last>2:
            state_file=ca.parent/'bridge-state'
            ready=not state_file.exists() or state_file.read_text().strip()=='ready'
            show='chat' if ready else 'dnd'
            stream.send("<presence to='alice@fixture.test'><show>"+show+"</show><status>Synthetic bridge state</status></presence>")
            last=time.monotonic()
        try:stream.next()
        except socket.timeout:pass
    stream.send('</stream:stream>');stream.sock.close()


def echo_peer(ca,stop):
    stream=login('bob',ca)
    stream.send('<presence/>')
    stream.sock.settimeout(1)
    seen=set()
    count=0
    print('Synthetic peer ready',flush=True)
    while not stop.is_set():
        try:stanza=stream.next()
        except socket.timeout:continue
        if stanza.tag!='{jabber:client}message' or stanza.get('type') not in ('chat','normal'):continue
        body=stanza.findtext('{jabber:client}body','')
        if not body:continue
        identifier=stanza.get('id','')
        target=stanza.get('from','')
        if not target.startswith('alice@fixture.test/'):continue
        count+=1
        duplicate=identifier in seen
        seen.add(identifier)
        print('Synthetic message',count,'has-id',bool(identifier),'duplicate',duplicate,flush=True)
        attributes=' to='+quoteattr(target)+' id='+quoteattr(identifier)
        if body.startswith('fail'):
            echoed='<body>'+escape(body)+'</body>' if body=='fail-body' else ''
            stream.send('<message type="error"'+attributes+'>'+echoed+"<error type='wait'><recipient-unavailable xmlns='urn:ietf:params:xml:ns:xmpp-stanzas'/></error></message>")
        elif body=='unconfirmed':
            pass
        else:
            stream.send('<message type="chat"'+attributes+"><received xmlns='urn:xmpp:receipts' id="+quoteattr(identifier)+"/></message>")
            oob=stanza.find('{jabber:x:oob}x')
            extension=ET.tostring(oob,encoding='unicode') if oob is not None else ''
            stream.send('<message type="chat" to='+quoteattr(target)+"><body>Synthetic echo</body>"+extension+'</message>')
    stream.send('</stream:stream>');stream.sock.close()

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('fixture',type=Path);args=parser.parse_args()
    stop=threading.Event()
    threads=[threading.Thread(target=func,args=(args.fixture/'ca.pem',stop)) for func in (status_peer,echo_peer)]
    for thread in threads:thread.start()
    try:
        while all(t.is_alive() for t in threads):time.sleep(.5)
    except KeyboardInterrupt:pass
    finally:
        stop.set()
        for thread in threads:thread.join(timeout=3)
