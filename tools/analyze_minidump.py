import struct, sys
p=sys.argv[1]
b=open(p,'rb').read()
sig,ver,n,rva,chk,timestamp,flags=struct.unpack_from('<IIIIIIQ',b,0)
print('dump',p,'bytes',len(b),'streams',n,'timestamp',timestamp)
streams={}
for i in range(n):
    typ,size,loc=struct.unpack_from('<III',b,rva+i*12); streams[typ]=(size,loc)
print('stream_types',sorted(streams))
def readstr(off):
    if not off:return ''
    ln=struct.unpack_from('<I',b,off)[0]
    try:return b[off+4:off+4+ln].decode('utf-16le','replace')
    except:return ''
mods=[]
if 4 in streams:
    sz,off=streams[4]; count=struct.unpack_from('<I',b,off)[0]
    for i in range(count):
        q=off+4+i*108
        base, size, csum, ts, name=struct.unpack_from('<QIIII',b,q)
        cvsz,cvrva=struct.unpack_from('<II',b,q+76)
        mods.append((base,base+size,readstr(name),cvsz,cvrva))
    for m in mods:
        if 'camera_emulator' in m[2].lower() or 'Qt6' in m[2] or 'ntdll' in m[2].lower():
            print('module',hex(m[0]),hex(m[1]),m[2],'cv',m[3],hex(m[4]))
mem=[]
if 5 in streams:
    sz,off=streams[5]; count=struct.unpack_from('<I',b,off)[0]
    for i in range(count):
        start,size,loc=struct.unpack_from('<QII',b,off+4+i*16); mem.append((start,size,loc))
if 9 in streams:
    sz,off=streams[9]; count,base=struct.unpack_from('<QQ',b,off)
    cur=off+16
    for i in range(count):
        start,size=struct.unpack_from('<QQ',b,cur+i*16); mem.append((start,size,base)); base+=size
print('memory_ranges',len(mem))
def memread(addr,n):
    for start,size,loc in mem:
        if start<=addr and addr+n<=start+size:return b[loc+(addr-start):loc+(addr-start)+n]
    return None
if 6 in streams:
    sz,off=streams[6]
    tid=struct.unpack_from('<I',b,off)[0]
    er=off+8
    code,eflags,rec,addr,np=struct.unpack_from('<IIQQI',b,er)
    nparams=struct.unpack_from('<I',b,er+24)[0]
    params=struct.unpack_from('<15Q',b,er+32)
    crva=struct.unpack_from('<I',b,off+164)[0]
    print('exception thread',tid,'code',hex(code),'address',hex(addr),'record',hex(rec),'contextRva',hex(crva),'params',nparams,[hex(x) for x in params[:nparams]])
    if crva:
        cflags=struct.unpack_from('<I',b,crva+48)[0]
        regs={name:struct.unpack_from('<Q',b,crva+at)[0] for name,at in [('Rax',120),('Rcx',128),('Rdx',136),('Rbx',144),('Rsp',152),('Rbp',160),('Rsi',168),('Rdi',176),('R8',184),('R9',192),('R10',200),('R11',208),('R12',216),('R13',224),('R14',232),('R15',240),('Rip',248)]}
        print('context_flags',hex(cflags),'registers', ' '.join(k+'='+hex(v) for k,v in regs.items()))
        rsp=regs['Rsp']; raw=memread(rsp,0x400)
        if raw:
            print('stack qwords:')
            for i in range(0,min(len(raw),0x400),8):
                val=struct.unpack_from('<Q',raw,i)[0]
                m=next((x for x in mods if x[0]<=val<x[1]),None)
                if m: print(' ',hex(rsp+i),hex(val),m[2],'+',hex(val-m[0]))
        else: print('stack memory unavailable at rsp')
