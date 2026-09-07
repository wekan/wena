#!/usr/bin/env python3
import argparse,json,re,subprocess
from pathlib import Path
ENV=[re.compile(r"process\.env(?:\.([A-Z][A-Z0-9_]*)|\[['\"]([A-Z][A-Z0-9_]*)['\"]\])"),re.compile(r"\$\{([A-Z][A-Z0-9_]*)(?=[:}])"),re.compile(r"^\s*-\s*([A-Z][A-Z0-9_]*)=")]
ROUTE=re.compile(r"WebApp\.handlers\.(get|post|put|patch|delete|options)\(\s*['\"]([^'\"]+)['\"]")
EXT={'.js','.cjs','.mjs','.sh','.yml','.yaml'}
def main():
 p=argparse.ArgumentParser();p.add_argument('--wekan-root',required=True);p.add_argument('--output',required=True);a=p.parse_args();root=Path(a.wekan_root).resolve();env={};routes=[];providers={x:[] for x in ('ldap','oauth2_oidc','cas','saml','password')}
 candidates=[]
 for name in ('server','imports','models','packages','client','releases'):
  base=root/name
  if base.exists():candidates.extend(base.rglob('*'))
 candidates.extend(f for f in root.iterdir() if f.is_file())
 for f in sorted(set(candidates)):
  if not f.is_file() or f.suffix not in EXT or any(x in f.parts for x in ('.git','.tools','node_modules','_build')):continue
  rel=f.relative_to(root).as_posix()
  try: lines=f.read_text(encoding='utf-8').splitlines()
  except UnicodeDecodeError: continue
  for no,line in enumerate(lines,1):
   for pattern in ENV:
    for m in pattern.finditer(line):
     name=next((x for x in m.groups() if x),None)
     if name:env.setdefault(name,[]).append(f'{rel}:{no}')
   for m in ROUTE.finditer(line):routes.append({'method':m.group(1).upper(),'path':m.group(2),'source':f'{rel}:{no}'})
  lower=rel.lower()
  for name,needles in {'ldap':('ldap',),'oauth2_oidc':('oauth2','oidc'),'cas':('/cas','accounts-cas'),'saml':('saml',),'password':('apiauthroutes','passwordlogin')}.items():
   if any(n in lower for n in needles):providers[name].append(rel)
 revision=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
 data={'schema':1,'wekan_revision':revision,'environment':[{'name':k,'sources':sorted(set(v))} for k,v in sorted(env.items())],'http_routes':sorted(routes,key=lambda x:(x['path'],x['method'],x['source'])),'login_provider_sources':{k:sorted(set(v)) for k,v in providers.items()}}
 Path(a.output).write_text(json.dumps(data,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
if __name__=='__main__':main()
