#include "capability.h"

#include <stddef.h>
#include <string.h>

const char *wena_legacy_html4_capability_script(void)
{
    static const char *const parts[] = {
        "(function(){'use strict';\nvar hidden=[];\n",
        "function restore(){var i,x;for(i=0;i<hidden.length;i+=1){x=hidden[i];x.base.style.display=x.display;x.base.removeAttribute('aria-hidden');x.drag.removeAttribute('aria-describedby');}hidden=[];}\n",
        "function capable(doc){var s,dt,seen=false,e;try{s=doc.createElement('div');if(!('draggable' in s)||typeof window.DragEvent!=='function'||typeof window.DataTransfer!=='function'||",
        "typeof window.AbortController!=='function'||typeof window.fetch!=='function'){return false;}dt=new window.DataTransfer();dt.setData('text/plain','wena');s.addEventListener('dragstart',",
        "function(ev){seen=ev.dataTransfer.getData('text/plain')==='wena';});e=new window.DragEvent('dragstart',{dataTransfer:dt});s.dispatchEvent(e);return seen;}catch(ignore){return false;}}\n",
        "function activate(doc){var all,i,base,drag,id,active;restore();if(!capable(doc)){return false;}all=doc.getElementsByClassName('wena-move-baseline');active=doc.activeElement;",
        "for(i=0;i<all.length;i+=1){base=all[i];id=base.id||'';if(id.slice(-9)!=='-baseline'){continue;}drag=doc.getElementById(id.slice(0,-9)+'-drag');if(!drag){continue;}",
        "drag.setAttribute('role','button');drag.setAttribute('tabindex','0');drag.setAttribute('aria-describedby',id);if(base.contains&&base.contains(active)&&drag.focus){drag.focus();}",
        "hidden.push({base:base,drag:drag,display:base.style.display});base.setAttribute('aria-hidden','true');base.style.display='none';}return hidden.length>0;}\n",
        "function guard(promise,milliseconds){var timer;return new Promise(function(resolve,reject){timer=window.setTimeout(function(){restore();reject(new Error('enhancement timeout'));},milliseconds);",
        "promise.then(function(value){window.clearTimeout(timer);resolve(value);},function(error){window.clearTimeout(timer);restore();reject(error);});});}\n",
        "window.WenaLegacyEnhancement={activate:activate,restore:restore,guard:guard};\nwindow.addEventListener('wena:enhancement-failed',restore);\n",
        "if(document.readyState==='loading'){document.addEventListener('DOMContentLoaded',function(){activate(document);});}else{activate(document);}\n}());\n"
    };
    static char script[4096];
    size_t index;
    if (script[0] == '\0') {
        for (index = 0; index < sizeof(parts) / sizeof(parts[0]); ++index) {
            if (strlen(script) + strlen(parts[index]) >= sizeof(script)) return "";
            strcat(script, parts[index]);
        }
    }
    return script;
}
