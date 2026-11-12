"""Offline persistence regression using real adapter/bookmark methods and SQLite."""
import asyncio
import inspect
import logging
import unittest
from types import SimpleNamespace as NS
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from slixmpp import JID, ClientXMPP
from slixmpp.plugins.xep_0060.stanza import Item
from slidge.db.meta import Base
from slidge.db.models import GatewayUser, Room, Avatar as StoredAvatar
from slidge.core.mixins.db import DBMixin
from slidge.group.room import LegacyMUC
from slidgnal.group import MUC
from slidgnal.generated import signal, go

class Tests(unittest.IsolatedAsyncioTestCase):
    async def test_group_photo_survives_bookmark_participant_refresh(self):
        client=ClientXMPP("fixture@example.test","unused-offline")
        client.register_plugin("xep_0402")
        fixed=inspect.getsource(MUC.update_info).find('await self.add_to_bookmarks()') < inspect.getsource(MUC.update_info).find('if data.Avatar.Data:')
        for initial,mode,expected in [(None,'new',2),(1,'new',2),(1,'delete',None),(1,'partial',1)]:
            with self.subTest(initial=initial,mode=mode):
                engine=create_engine('sqlite://')
                Base.metadata.create_all(engine)
                sessions=sessionmaker(engine,expire_on_commit=False)
                with sessions() as db:
                    user=GatewayUser(jid=JID('alice@example.test'))
                    db.add_all([user,StoredAvatar(id=1,hash='old',width=1,height=1),StoredAvatar(id=2,hash='new',width=1,height=1)])
                    db.flush();db.add(Room(id=1,user=user,legacy_id='synthetic',jid_localpart='synthetic',name='Fixture',avatar_id=initial));db.commit()
                def load():
                    with sessions() as db:
                        room=db.get(Room,1);_ = room.avatar
                        return room
                published=[]
                async def publish(iq):published.append(iq)
                old=Item();old['conference']['nick']='self';old['conference']['name']='Fixture'
                async def existing():return old
                room=NS(stored=load(),_updating_info=True,user_nick='self',name='Fixture',
                    jid=JID('synthetic@signal.example.test'),user_jid=JID('alice@example.test'),
                    get_lock=lambda key:False,log=logging.getLogger('test'),
                    session=NS(user=NS(preferences={'always_invite_when_adding_bookmarks':False})),
                    _LegacyMUC__get_bookmark=existing,_set_expire_timer=lambda timer:None)
                room.xmpp=NS(store=NS(session=sessions),plugin={'xep_0356':NS(send_privileged_iq=publish)})
                # The real bookmark path stores a participant and replaces the
                # room object with its independently loaded ORM relationship.
                part=NS(jid=JID('synthetic@signal.example.test/self'),stored=NS(room=load()))
                def participant_commit():part.stored.room=load()
                part.commit=participant_commit
                async def participant(*args,**kwargs):return part
                room.get_participant=participant
                room._LegacyMUC__store_participant=lambda p:LegacyMUC._LegacyMUC__store_participant(room,p)
                room.get_user_participant=lambda:LegacyMUC.get_user_participant(room)
                room.add_to_bookmarks=lambda:LegacyMUC.add_to_bookmarks(room)
                async def set_avatar(avatar):
                    if avatar is None:room.stored.avatar=None
                    else:
                        self.assertEqual(avatar.data,b'synthetic-picture')
                        with sessions() as db:room.stored.avatar=db.get(StoredAvatar,2)
                room.set_avatar=set_avatar
                data=signal.Group(Title='Fixture')
                if mode=='new':data.Avatar=signal.Avatar(Data=go.Slice_byte.from_bytes(b'synthetic-picture'))
                elif mode=='delete':data.Avatar=signal.Avatar(Delete=True)
                await MUC.update_info(room,data)
                DBMixin.commit(room)
                actual=load().avatar_id
                self.assertEqual(actual,expected if fixed else initial)
                if not fixed and mode!='partial':print('BASELINE: bookmark refresh discards pending group avatar',mode)
                engine.dispose()

if __name__=='__main__':unittest.main()
