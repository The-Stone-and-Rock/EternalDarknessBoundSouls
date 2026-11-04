import json,re,collections
t=json.load(open('build/ed-db/text.json'))
names=['Pious','Ellia','Anthony','Karim','Maximillian','Paul','Peter','Roberto','Edward','Michael','Alexandra','Alex ','Lindsey','Inspector','Bianchi','Luther','Jacob','Augustus','Edwards','Roivas']
for k in sorted(t):
    if not k.startswith('room'): continue
    c=collections.Counter()
    for key,v in t[k].items():
        s=v[2] if isinstance(v,list) else str(v)
        for n in names:
            if n in s: c[n]+=1
    print(k,len(t[k]),c.most_common(8))
