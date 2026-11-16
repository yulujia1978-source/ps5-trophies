"""Synthetic layout fixtures. These are not console dumps."""
import json, struct
from pathlib import Path

def trp(rows=((0,0,0),(1,1,63900000000000000)), count=2):
    data=bytearray(0x800+count*0xd0+len(rows)*0x60)
    data[:4]=b'T2PD'
    for i in range(count):
        off=0x200+i*0xd0
        struct.pack_into('>II',data,off,0x500,0xc0)
        struct.pack_into('>I',data,off+16,i)
    for index,(tid,flag,stamp) in enumerate(rows):
        off=0x800+index*0x60
        struct.pack_into('>II',data,off,0x800,0x50)
        struct.pack_into('>II',data,off+16,tid,flag)
        struct.pack_into('>Q',data,off+32,stamp)
    return data

def ucp(entries):
    data=bytearray(0x60+len(entries)*64)
    struct.pack_into('>I',data,0,0xb228c60a)
    struct.pack_into('>II',data,16,len(entries),0x40)
    for i,(name,blob) in enumerate(entries.items()):
        if isinstance(blob,str):blob=blob.encode()
        e=0x60+i*64
        data[e:e+len(name)]=name.encode()
        struct.pack_into('>QQ',data,e+32,len(data),len(blob))
        data+=blob
    return data

def create(root):
    root=Path(root)
    conf={'trophies':[{'id':'0000','grade':'P','groupId':'000','hidden':False},{'id':'0001','grade':'B','groupId':'000','hidden':False}]}
    meta={'metadata':{'titleMetadata':{'name':'本地测试游戏'},'groupMetadata':[{'id':'000','name':'正式版游戏'}],'trophyMetadata':[{'id':'0000','name':'全部达成','detail':'获得全部奖杯。'},{'id':'0001','name':'初次冒险','detail':'完成第一次挑战。'}]}}
    archive=root/'user/trophy2/nobackup/conf/NPWR00001_00/TROPHY.UCP'
    archive.parent.mkdir(parents=True,exist_ok=True)
    archive.write_bytes(ucp({'tropconf.json':json.dumps(conf),'tropmeta_zh-Hans.json':json.dumps(meta,ensure_ascii=False)}))
    for uid,sets in {'10000000':{'NPWR00001_00':trp(),'NPWR00002_00':trp(((0,0,0),(1,0,0))),'NPWR00003_00':b'unsupported'},'20000000':{'NPWR00001_00':trp(((0,0,0),(1,0,0)))}}.items():
        for name,data in sets.items():
            dest=root/f'user/home/{uid}/trophy2/nobackup/data/{name}/TRPTITLE.DAT'
            dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(data)

if __name__=='__main__':
    import sys
    create(sys.argv[1])
