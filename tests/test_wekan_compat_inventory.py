import json,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];WEKAN=ROOT.parents[1];LOCK=ROOT/'config/wekan-compat-inventory.json'
with tempfile.TemporaryDirectory() as d:
 out=Path(d)/'inventory.json';subprocess.run(['python3',str(ROOT/'scripts/generate_wekan_compat_inventory.py'),'--wekan-root',str(WEKAN),'--output',str(out)],check=True)
 assert out.read_bytes()==LOCK.read_bytes(),'WeKan compatibility inventory is stale'
 data=json.loads(out.read_text());names={x['name'] for x in data['environment']}
 assert len(names)>=200 and {'ROOT_URL','LDAP_ENABLE','OAUTH2_ENABLED','CAS_ENABLED'}<=names
 routes={(x['method'],x['path']) for x in data['http_routes']}
 assert ('POST','/users/login') in routes and ('POST','/users/logout') in routes
 assert all(data['login_provider_sources'][x] for x in ('ldap','oauth2_oidc','cas','saml','password'))
