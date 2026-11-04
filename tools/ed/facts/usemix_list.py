import sys,json,struct
sys.path.insert(0,'tools/ed/facts'); sys.path.insert(0,'tools/ed')
import usemix_inv
T=json.load(open('build/ed-db/text.json',encoding='utf8'))['boot']
R=usemix_inv.load()
for i,r in enumerate(R):
    w0,w4=struct.unpack('>II',r[:8])
    s36,s38,s40,s42=struct.unpack('>hhhh',r[36:44])
    g,p,d,n=r[76:80]
    c=struct.unpack('>HHH',r[68:74])
    row=T.get('%d,%d'%(g,n),['?','?','?'])
    print(i,'al=%x fl=%x'%(w0,w4),'pk%d use%d s40=%d mix%d'%(s36,s38,s40,s42),'g%d.%d.%d.%d'%(g,p,d,n),'cnt',c,'B',list(r[80:86]),row[1],'|',row[2][:30])
