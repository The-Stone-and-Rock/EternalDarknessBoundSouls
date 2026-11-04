"""Label form of each row, unused _INV labels, InvS +20 and masks by kind; read only."""
import sys,json,os,re,collections,struct
HERE=os.path.dirname(os.path.abspath(__file__));ROOT=os.path.abspath(os.path.join(HERE,'..','..','..'))
J=json.load(open(os.path.join(ROOT,'build/ed-db/facts/item_table.json'),encoding='utf8'))
rows=J['rows']
fine=before=0;odd=[]
for r in rows:
    p,d,n=r['text']['labels']
    a=(p.endswith('_PICKUP'),d.endswith('_DESC'),n.endswith('_INV'))
    if all(a): fine+=1
    elif a[0] and a[2] and d.endswith('_BEFORE'): before+=1;odd.append((r['id'],'before',p,d,n))
    else: odd.append((r['id'],'odd',p,d,n,r['placeholder']))
print('fine',fine,'before',before,'others',len(odd)-before,'placeholders',sum(r['placeholder'] for r in rows))
for o in odd: print(' ',o)
print('unused _INV',len(J['item_labels_with_no_row']),J['item_labels_with_no_row'][:40])
for i,k in enumerate(J['inv_kinds']):
    if k['fire_modes'] or k['mask']: print('kind',i,'fire_modes',hex(k['fire_modes']),'mask',hex(k['mask']))
print('nonzero +0:',[(r['id'],r['name'],r['record']['fire_mode'],r['inv_kinds']) for r in rows if r['record']['fire_mode']])
print('text_enchanted',collections.Counter(r['record']['text_enchanted'] for r in rows))
print('after_examine',[(r['id'],r['name'],r['after_examine_script']) for r in rows if r['after_examine_script']])
print('flag20',[(r['id'],r['name'],r['use_script']) for r in rows if r['record']['flags']&0x20][:30])
print('no-place', sum(1 for r in rows if not r['inv_kinds']), [r['id'] for r in rows if not r['inv_kinds']])
print('partners',[(r['id'],r['name'],r['reload_partners'],r['inv_kinds']) for r in rows if r['text']['group'] in (3,4) and not r['placeholder']])
for i in (102,9,11,121,122,209,210,211,212,43,57,47,56,32,33,136,137,135,97,45,91,243):
    r=rows[i];print(i,r['label'],repr(r['name']),'use',r['use_script'],'find',r['after_find_script'],'mix',r['mix_script'],'cnt',r['count'],r['capacity'],'g',r['text']['group'])
