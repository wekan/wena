#include "capability.h"

#include <stddef.h>
#include <string.h>

const char *wena_legacy_html4_capability_script(void)
{
    static const char *const parts[] = {
        "(function(){'use strict';\nvar hidden=[];\n",
        "function restore(){var i,x;for(i=0;i<hidden.length;i+=1){x=hidden[i];x.base.style.display=x.display;x.base.removeAttribute('aria-hidden');x.drag.removeAttribute('aria-describedby');}hidden=[];}\n",
        "function capable(doc){var s,dt,seen=false,e;try{s=doc.createElement('div');if(!('draggable' in s)||typeof window.DragEvent!=='function'||typeof window.DataTransfer!=='function'||",
        "typeof window.AbortController!=='function'||typeof window.fetch!=='function'||typeof window.TextDecoder!=='function'){return false;}dt=new window.DataTransfer();dt.setData('text/plain','wena');s.addEventListener('dragstart',",
        "function(ev){seen=ev.dataTransfer.getData('text/plain')==='wena';});e=new window.DragEvent('dragstart',{dataTransfer:dt});s.dispatchEvent(e);return seen;}catch(ignore){return false;}}\n",
        "function activate(doc){var all,i,base,drag,id,active;restore();if(!capable(doc)){return false;}all=doc.getElementsByClassName('wena-move-baseline');active=doc.activeElement;",
        "for(i=0;i<all.length;i+=1){base=all[i];id=base.id||'';if(id.slice(-9)!=='-baseline'){continue;}drag=doc.getElementById(id.slice(0,-9)+'-drag');if(!drag){continue;}",
        "drag.setAttribute('role','button');drag.setAttribute('tabindex','0');drag.setAttribute('aria-describedby',id);if(base.contains&&base.contains(active)&&drag.focus){drag.focus();}",
        "hidden.push({base:base,drag:drag,display:base.style.display});base.setAttribute('aria-hidden','true');base.style.display='none';}return hidden.length>0;}\n",
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
        "window.WenaLegacyEnhancement={activate:activate,restore:restore,guard:guard,parseRegions:parseRegions,applyRegions:applyRegions};\nwindow.addEventListener('wena:enhancement-failed',restore);\n",
        "if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',function(){activate(document);});}else{activate(document);}\n}());\n"
    };
    static char script[8192];
    size_t index;
    if (script[0] == '\0') {
        for (index = 0; index < sizeof(parts) / sizeof(parts[0]); ++index) {
            if (strlen(script) + strlen(parts[index]) >= sizeof(script)) return "";
            strcat(script, parts[index]);
        }
    }
    return script;
}
