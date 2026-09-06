#include "capability.h"

#include <stddef.h>
#include <string.h>

const char *wena_legacy_html4_capability_stylesheet(void)
{
    return ".wena-drag-control{display:none}.wena-enhanced .wena-drag-control{display:inline}";
}

const char *wena_legacy_html4_capability_script(void)
{
    static const char *const parts[] = {
        "(function(){'use strict';\nvar hidden=[];\n",
        "function restore(){var i,x;for(i=0;i<hidden.length;i+=1){x=hidden[i];x.base.style.display=x.display;x.base.removeAttribute('aria-hidden');x.drag.removeAttribute('aria-describedby');}hidden=[];document.body.className=document.body.className.replace(/ ?wena-enhanced/g,'');}\n",
        "function capable(doc){var s,dt,seen=false,e;try{s=doc.createElement('div');if(!('draggable' in s)||typeof window.DragEvent!=='function'||typeof window.DataTransfer!=='function'||",
        "typeof window.AbortController!=='function'||typeof window.fetch!=='function'||typeof window.TextDecoder!=='function'||typeof window.FormData!=='function'||typeof window.URLSearchParams!=='function'){return false;}dt=new window.DataTransfer();dt.setData('text/plain','wena');s.addEventListener('dragstart',",
        "function(ev){seen=ev.dataTransfer.getData('text/plain')==='wena';});e=new window.DragEvent('dragstart',{dataTransfer:dt});s.dispatchEvent(e);return seen;}catch(ignore){return false;}}\n",
        "function activate(doc){var all,i,base,drag,id,active;restore();if(!capable(doc)){return false;}all=doc.getElementsByClassName('wena-move-baseline');active=doc.activeElement;",
        "for(i=0;i<all.length;i+=1){base=all[i];id=base.id||'';if(id.slice(-9)!=='-baseline'){continue;}drag=doc.getElementById(id.slice(0,-9)+'-drag');if(!drag){continue;}",
        "drag.setAttribute('role','button');drag.setAttribute('tabindex','0');drag.setAttribute('aria-describedby',id);if(base.contains&&base.contains(active)&&drag.focus){drag.focus();}",
        "hidden.push({base:base,drag:drag,display:base.style.display});base.setAttribute('aria-hidden','true');base.style.display='none';}if(hidden.length>0){doc.body.className+=' wena-enhanced';}return hidden.length>0;}\n",
        "function guard(promise,milliseconds){var timer;return new Promise(function(resolve,reject){timer=window.setTimeout(function(){restore();reject(new Error('enhancement timeout'));},milliseconds);",
        "promise.then(function(value){window.clearTimeout(timer);resolve(value);},function(error){window.clearTimeout(timer);restore();reject(error);});});}\n",
        "function ascii(bytes,start,end){var s='',i;for(i=start;i<end;i+=1){if(bytes[i]>127){throw new Error('non-ASCII frame');}s+=String.fromCharCode(bytes[i]);}return s;}\n",
        "function safeNumber(value){var n=Number(value);if(!isFinite(n)||n<1||n>9007199254740991||Math.floor(n)!==n){throw new Error('invalid region number');}return n;}\n",
        "function parseRegions(buffer){var b=new Uint8Array(buffer),p=0,line,match,count=0,names={},regions=[],decoder=new window.TextDecoder('utf-8',{fatal:true});if(b.length>32768){throw new Error('region response too large');}",
        "function next(){var start=p;while(p<b.length&&b[p]!==10){p+=1;}if(p>=b.length){throw new Error('malformed region frame');}line=ascii(b,start,p);p+=1;return line;}if(next()!=='WENA-REGIONS/1'){throw new Error('unknown region schema');}",
        "match=/^request-version ([1-9][0-9]*)$/.exec(next());if(!match){throw new Error('invalid request version');}var requestVersion=safeNumber(match[1]);while(p<b.length){line=next();if(line==='end'){if(p!==b.length){throw new Error('trailing region data');}return {requestVersion:requestVersion,regions:regions};}",
        "match=/^region ((?:board|sidebar|card-details)|(?:card|list|swimlane)-[A-Za-z0-9_-]+) ([1-9][0-9]*) ([1-9][0-9]*)$/.exec(line);if(!match||Object.prototype.hasOwnProperty.call(names,match[1])||count>=8){throw new Error('invalid region header');}var version=safeNumber(match[2]),length=safeNumber(match[3]);if(length>4096||p+length>=b.length||b[p+length]!==10){throw new Error('invalid region length');}",
        "var content=decoder.decode(b.slice(p,p+length));p+=length+1;names[match[1]]=true;regions.push({name:match[1],version:version,content:content});count+=1;}throw new Error('missing region end');}\n",
        "function applyRegions(buffer,expected,state,doc){var parsed,changes=[],i,r,current,node;try{parsed=parseRegions(buffer);if(parsed.requestVersion!==expected||parsed.requestVersion<=state.lastRequestVersion){throw new Error('stale request');}",
        "for(i=0;i<parsed.regions.length;i+=1){r=parsed.regions[i];if(!Object.prototype.hasOwnProperty.call(state.visible,r.name)){throw new Error('unknown visible region');}current=state.visible[r.name];if(r.version<=current.version){throw new Error('stale region');}node=doc.getElementById(current.elementId);if(!node){throw new Error('missing visible region');}changes.push({region:r,current:current,node:node});}",
        "for(i=0;i<changes.length;i+=1){changes[i].node.textContent=changes[i].region.content;changes[i].current.version=changes[i].region.version;}state.lastRequestVersion=parsed.requestVersion;return parsed;}catch(error){restore();throw error;}}\n",
        "function sameOrigin(url){var link=document.createElement('a');link.href=url;return link.protocol===window.location.protocol&&link.host===window.location.host;}\n",
        "function postForm(form,state,doc,milliseconds){var action=form.getAttribute('action'),method=(form.getAttribute('method')||'').toUpperCase(),controller,version,body,promise;if(method!=='POST'||!sameOrigin(action)||state.inFlight){restore();return Promise.reject(new Error('unsafe enhancement request'));}",
        "controller=new window.AbortController();version=state.nextRequestVersion;if(typeof version!=='number'||version<=state.lastRequestVersion){restore();return Promise.reject(new Error('invalid request sequence'));}state.inFlight=true;body=new window.URLSearchParams(new window.FormData(form));",
        "promise=window.fetch(action,{method:'POST',credentials:'same-origin',redirect:'error',headers:{'Accept':'application/vnd.wena.regions-v1','Content-Type':'application/x-www-form-urlencoded;charset=UTF-8','X-Wena-Request-Version':String(version)},body:body.toString(),signal:controller.signal})",
        ".then(function(response){var length=response.headers.get('Content-Length'),type=(response.headers.get('Content-Type')||'').split(';')[0];if(!response.ok||type!=='application/vnd.wena.regions-v1'||(length!==null&&safeNumber(length)>32768)){throw new Error('invalid enhancement response');}return response.arrayBuffer();})",
        ".then(function(buffer){if(buffer.byteLength>32768){throw new Error('region response too large');}return applyRegions(buffer,version,state,doc);});return guard(promise,milliseconds).then(function(value){state.nextRequestVersion=version+1;state.inFlight=false;return value;},function(error){controller.abort();state.inFlight=false;restore();throw error;});}\n",
        "function field(form,name){var nodes=form.elements,i,found=null;for(i=0;i<nodes.length;i+=1){if(nodes[i].name===name){if(found!==null||!nodes[i].value){return null;}found=nodes[i];}}return found;}\n",
        "function signedMoveForm(doc,id){var form=doc.getElementById(id);if(!form||String(form.tagName).toUpperCase()!=='FORM'||form.className.indexOf('wena-move-baseline')<0||!field(form,'legacySession')||!field(form,'csrf')||!field(form,'legacyOperation')){return null;}return form;}\n",
        "function bindDragDrop(doc,state,milliseconds){var sources=doc.getElementsByClassName('wena-drag-source'),targets=doc.getElementsByClassName('wena-drop-target'),i,live=doc.getElementById('wena-drag-status');state.dragging=null;function fail(){state.dragging=null;if(live){live.textContent='Move failed; use move buttons.';}restore();}",
        "function submit(target,event){var form=signedMoveForm(doc,target.getAttribute('data-wena-form'));if(!state.dragging||!form){fail();return false;}try{event.preventDefault();postForm(form,state,doc,milliseconds).then(function(){state.dragging=null;if(live){live.textContent='Move complete.';}if(target.focus){target.focus();}},fail);return true;}catch(ignore){fail();return false;}}",
        "for(i=0;i<sources.length;i+=1){sources[i].setAttribute('draggable','true');sources[i].addEventListener('dragstart',function(event){try{state.dragging=this;event.dataTransfer.effectAllowed='move';event.dataTransfer.setData('application/x-wena-move','1');}catch(ignore){fail();}});sources[i].addEventListener('dragend',function(){state.dragging=null;});",
        "sources[i].addEventListener('keydown',function(event){if(event.key==='Enter'||event.key===' '){event.preventDefault();state.dragging=this;if(live){live.textContent='Move selected; choose a destination.';}}});}",
        "for(i=0;i<targets.length;i+=1){if(!signedMoveForm(doc,targets[i].getAttribute('data-wena-form'))){fail();return false;}targets[i].addEventListener('dragover',function(event){if(state.dragging){event.preventDefault();event.dataTransfer.dropEffect='move';}});targets[i].addEventListener('drop',function(event){if(event.dataTransfer.getData('application/x-wena-move')==='1'){submit(this,event);}else{fail();}});",
        "targets[i].addEventListener('keydown',function(event){if((event.key==='Enter'||event.key===' ')&&state.dragging){submit(this,event);}});}return sources.length>0&&targets.length>0;}\n",
        "function enhance(doc){var state={lastRequestVersion:0,nextRequestVersion:1,inFlight:false,visible:{board:{elementId:'wena-region-board',version:0}}};if(!activate(doc)){return false;}if(!bindDragDrop(doc,state,5000)){restore();return false;}return true;}\n",
        "window.WenaLegacyEnhancement={activate:activate,restore:restore,guard:guard,parseRegions:parseRegions,applyRegions:applyRegions,postForm:postForm,bindDragDrop:bindDragDrop};\nwindow.addEventListener('wena:enhancement-failed',restore);\n",
        "if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',function(){enhance(document);});}else{enhance(document);}\n}());\n"
    };
    static char script[16384];
    size_t index;
    if (script[0] == '\0') {
        for (index = 0; index < sizeof(parts) / sizeof(parts[0]); ++index) {
            if (strlen(script) + strlen(parts[index]) >= sizeof(script)) return "";
            strcat(script, parts[index]);
        }
    }
    return script;
}
