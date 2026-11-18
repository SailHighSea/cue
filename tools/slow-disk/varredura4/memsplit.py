import sys,re
pid=sys.argv[1]; lo=hi=0; big={}
cur=None
for l in open(f'/proc/{pid}/smaps'):
    m=re.match(r'([0-9a-f]+)-([0-9a-f]+) \S+ \S+ \S+ \S+\s*(.*)',l)
    if m: cur=(int(m.group(1),16),m.group(3)); continue
    if l.startswith('Rss:'):
        kb=int(l.split()[1])
        if cur[0] < 0x7f0000000000: lo+=kb
        else: hi+=kb
        name=cur[1] or ('anon-lo' if cur[0]<0x7f0000000000 else 'anon-hi')
        if 'llvm' in name or 'gallium' in name or 'dri' in name: name='mesa'
        big[name]=big.get(name,0)+kb
top=sorted(big.items(),key=lambda x:-x[1])[:4]
print(f"win {lo//1024}MB native {hi//1024}MB", ' '.join(f"{k.split('/')[-1]}={v//1024}" for k,v in top))
