/* Behavioral integration of the actual C-emitted asset. This minimal DOM is
 * deliberately not a claim of browser or accessibility end-to-end coverage. */
'use strict';
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const script = fs.readFileSync(process.argv[2], 'utf8');
let checks = 0;
function check(name, fn) { return Promise.resolve().then(fn).then(() => { checks++; console.log('ok - ' + name); }); }
function environment() {
  const nodes = [], timers = new Map(); let timerId = 0;
  const doc = { readyState: 'loading', body: { className: '' }, activeElement: null,
    addEventListener() {}, getElementById(id) { return nodes.find(n => n.id === id) || null; },
    getElementsByClassName(c) { return nodes.filter(n => n.className.split(' ').includes(c)); },
    createElement(tag) { const n = element('', '', tag); if (tag === 'a') Object.defineProperty(n, 'href', { set(value) { const u = new URL(value, 'https://wena.test/'); n.protocol = u.protocol; n.host = u.host; } }); return n; } };
  function element(id, className = '', tagName = 'DIV') {
    const attrs = {}, listeners = {};
    const n = { id, className, tagName, style: { display: '' }, draggable: false, textContent: 'before', elements: [],
      setAttribute(k,v) { attrs[k] = String(v); }, removeAttribute(k) { delete attrs[k]; }, getAttribute(k) { return attrs[k] ?? null; },
      contains(n2) { return n2 === n || n.elements.includes(n2); }, focus() { doc.activeElement = n; },
      addEventListener(k,fn) { (listeners[k] ||= []).push(fn); },
      dispatchEvent(e) { for (const f of listeners[e.type] || []) f.call(n,e); } };
    if (id) nodes.push(n); return n;
  }
  class DataTransfer { constructor() { this.data = {}; } setData(k,v) { this.data[k] = v; } getData(k) { return this.data[k] || ''; } }
  const win = { location: { protocol: 'https:', host: 'wena.test' }, DataTransfer,
    DragEvent: class { constructor(type, options) { this.type = type; Object.assign(this,options); } }, AbortController, TextDecoder, URLSearchParams,
    FormData: class { constructor(form) { this.entries = form.elements.map(n => [n.name,n.value]); } [Symbol.iterator]() { return this.entries[Symbol.iterator](); } },
    fetch() { throw Error('fetch not configured'); }, addEventListener() {},
    setTimeout(fn) { const id = ++timerId; timers.set(id,fn); return id; }, clearTimeout(id) { timers.delete(id); } };
  vm.runInNewContext(script, { window: win, document: doc, Promise, Uint8Array });
  function region(name) { const n = element('wena-region-' + name, 'wena-visible-region'); n.setAttribute('data-wena-region',name); n.setAttribute('data-wena-version','1'); return n; }
  const board = region('board'), sidebar = region('sidebar');
  const form = element('move-baseline', 'wena-move-baseline', 'FORM'); form.setAttribute('method','POST'); form.setAttribute('action','/move');
  form.elements = ['legacySession','csrf','legacyOperation'].map(name => ({name,value:'signed-' + name}));
  const drag = element('move-drag','wena-drag-source'), target = element('destination','wena-drop-target'); target.setAttribute('data-wena-form',form.id);
  const live = element('wena-drag-status');
  const api = win.WenaLegacyEnhancement;
  return { api, win, doc, board, sidebar, form, drag, target, live, timers,
    state: { lastRequestVersion: 0, nextRequestVersion: 1, inFlight: false, visible: api.visibleState(doc) } };
}
function frame(version = 1, regions = [['board',2,'updated']]) {
  const chunks = [Buffer.from('WENA-REGIONS/1\nrequest-version ' + version + '\n')];
  for (const [name,v,text] of regions) { const b = Buffer.isBuffer(text) ? text : Buffer.from(text); chunks.push(Buffer.from(`region ${name} ${v} ${b.length}\n`),b,Buffer.from('\n')); }
  chunks.push(Buffer.from('end\n')); const b = Buffer.concat(chunks); return b.buffer.slice(b.byteOffset,b.byteOffset+b.byteLength);
}
function response(buffer = frame(), status = true, type = 'application/vnd.wena.regions-v1') {
  return { ok: status, headers: { get(k) { return k === 'Content-Type' ? type : String(buffer.byteLength); } }, arrayBuffer() { return Promise.resolve(buffer); } };
}
async function flush() { for (let i=0;i<12;i++) await Promise.resolve(); }
function key(node) { const event = {type:'keydown',key:'Enter',preventDefault() { this.prevented = true; }}; node.dispatchEvent(event); assert.equal(event.prevented,true); }
(async () => {
  await check('valid UTF-8 and literal text-only atomic multi-region update', () => {
    const e = environment(); const content = '<img src=x onerror=alert(1)> Ää 日本語';
    e.api.applyRegions(frame(1,[['board',2,content],['sidebar',3,'second']]),1,e.state,e.doc);
    assert.equal(e.board.textContent,content); assert.equal(e.sidebar.textContent,'second'); assert.equal(e.state.visible.sidebar.version,3); assert.equal(e.state.lastRequestVersion,1);
  });
  await check('malformed, duplicate, oversized, unsafe-number and invalid UTF-8 frames fail', () => {
    const e = environment(); const bad = [Buffer.from('garbage'), Buffer.from('WENA-REGIONS/1\nrequest-version 1\n'), frame(1,[['board',2,Buffer.from([0xc3,0x28])]]), frame(1,[['board',2,'x'.repeat(4097)]]), frame(1,[['board',2,'a'],['board',3,'b']]), frame(1,[['arbitrary',2,'x']]), frame(9007199254740992), frame(1,[['board',9007199254740992,'x']]), Buffer.alloc(32769), Buffer.concat([Buffer.from(frame()),Buffer.from('extra')]), frame(1,Array.from({length:9},(_,i)=>['card-'+i,2,'x']))];
    for (const b of bad) assert.throws(() => e.api.parseRegions(b));
    assert.equal(e.board.textContent,'before');
  });
  await check('stale request, stale region, unknown and missing region reject atomically', () => {
    for (const [version,regions,expected,remove] of [[1,[['board',2,'new'],['sidebar',1,'stale']],1,false],[2,[['board',2,'new']],1,false],[1,[['board',2,'new'],['card-missing',2,'bad']],1,false],[1,[['board',2,'new'],['sidebar',2,'bad']],1,true]]) {
      const e=environment(); if(remove) e.sidebar.id='removed';
      assert.throws(() => e.api.applyRegions(frame(version,regions),expected,e.state,e.doc)); assert.equal(e.board.textContent,'before'); assert.equal(e.state.visible.board.version,1); assert.equal(e.state.lastRequestVersion,0);
    }
    const e=environment(); e.api.applyRegions(frame(),1,e.state,e.doc); assert.throws(()=>e.api.applyRegions(frame(),1,e.state,e.doc));
  });
  await check('capability activation transfers focus and restores baseline on failure', () => {
    const e=environment(); e.form.style.display='block'; e.doc.activeElement=e.form.elements[0];
    assert.equal(e.api.activate(e.doc),true); assert.equal(e.doc.activeElement,e.drag); assert.equal(e.form.style.display,'none');
    e.win.DataTransfer=undefined; assert.equal(e.api.activate(e.doc),false); assert.equal(e.form.style.display,'block'); assert.equal(e.form.getAttribute('aria-hidden'),null); assert.equal(e.drag.getAttribute('aria-describedby'),null); assert.equal(e.doc.body.className,'');
  });
  await check('POST preserves signed fields, advances sequence, and rejects replay', async () => {
    const e=environment(); let options; e.win.fetch=(url,o)=>{options=o; return Promise.resolve(response());};
    await e.api.postForm(e.form,e.state,e.doc,100); assert.equal(options.method,'POST'); assert.equal(options.credentials,'same-origin'); assert.equal(options.redirect,'error'); assert.equal(options.headers['X-Wena-Request-Version'],'1'); assert.match(options.body,/csrf=signed-csrf/); assert.equal(e.state.nextRequestVersion,2); assert.equal(e.state.inFlight,false);
    await assert.rejects(e.api.postForm(e.form,e.state,e.doc,100)); assert.equal(e.state.lastRequestVersion,1); assert.equal(options.signal.aborted,true);
  });
  await check('POST response/network errors abort and restore baseline', async () => {
    for (const mode of ['network','status','type','oversize']) { const e=environment(); let signal; e.api.activate(e.doc);
      e.win.fetch=(url,o)=>{signal=o.signal; return mode==='network'?Promise.reject(Error('offline')):Promise.resolve(response(mode==='oversize'?new ArrayBuffer(32769):frame(),mode!=='status',mode==='type'?'text/html':'application/vnd.wena.regions-v1'));};
      await assert.rejects(e.api.postForm(e.form,e.state,e.doc,100)); assert.equal(signal.aborted,true); assert.equal(e.state.inFlight,false); assert.equal(e.form.style.display,''); assert.equal(e.board.textContent,'before');
    }
  });
  await check('unsafe cross-origin, GET and concurrent POST requests are rejected', async () => {
    for(const mode of ['origin','method','busy']) {const e=environment(); if(mode==='origin')e.form.setAttribute('action','https://other.test/move');if(mode==='method')e.form.setAttribute('method','GET');if(mode==='busy')e.state.inFlight=true; await assert.rejects(e.api.postForm(e.form,e.state,e.doc,100));}
  });
  await check('timeout abort ignores late response and leaves persisted UI state unchanged', async () => {
    const e=environment(); let complete,signal;
    e.win.fetch=(url,o)=>{signal=o.signal;return new Promise(resolve=>{complete=resolve;});};
    const pending=e.api.postForm(e.form,e.state,e.doc,100); const rejected=assert.rejects(pending,/timeout/); await flush();
    for(const fn of [...e.timers.values()])fn(); await rejected; assert.equal(signal.aborted,true);
    complete(response()); await flush(); assert.equal(e.board.textContent,'before'); assert.equal(e.state.lastRequestVersion,0); assert.equal(e.state.inFlight,false);
  });
  await check('timeout during response body decoding cannot overwrite a successful retry', async () => {
    const e=environment(); let finishBody, calls=0;
    e.win.fetch=()=>{ calls++; if(calls>1)return Promise.resolve(response(frame(1,[['board',2,'retry result']]))); const r=response(); r.arrayBuffer=()=>new Promise(resolve=>{finishBody=resolve;});return Promise.resolve(r); };
    const pending=e.api.postForm(e.form,e.state,e.doc,100);const rejected=assert.rejects(pending,/timeout/);await flush();assert.equal(typeof finishBody,'function');
    for(const fn of [...e.timers.values()])fn();await rejected;
    await e.api.postForm(e.form,e.state,e.doc,100);finishBody(frame(1,[['board',3,'late old result']]));await flush();
    assert.equal(e.board.textContent,'retry result');assert.equal(e.state.visible.board.version,2);assert.equal(e.state.lastRequestVersion,1);assert.equal(e.state.nextRequestVersion,2);
  });
  await check('synchronous transport and form serialization errors release inFlight', async () => {
    for (const mode of ['fetch','form']) {const e=environment(); if(mode==='form')e.win.FormData=class {constructor(){throw Error('form unavailable');}};
      await assert.rejects(Promise.resolve().then(()=>e.api.postForm(e.form,e.state,e.doc,100))); assert.equal(e.state.inFlight,false);assert.equal(e.board.textContent,'before');}
  });
  await check('keyboard move posts signed destination and focuses target', async () => {
    const e=environment(); e.win.fetch=()=>Promise.resolve(response()); assert.equal(e.api.bindDragDrop(e.doc,e.state,100),true);
    key(e.drag); assert.equal(e.live.textContent,'Move selected; choose a destination.'); key(e.target); await flush(); assert.equal(e.live.textContent,'Move complete.'); assert.equal(e.doc.activeElement,e.target); assert.equal(e.state.dragging,null); assert.equal(e.board.textContent,'updated');
  });
  await check('pointer drag transfers move marker and handles invalid external drops', async () => {
    const e=environment();e.api.activate(e.doc);e.win.fetch=()=>Promise.resolve(response());e.api.bindDragDrop(e.doc,e.state,100);
    const dataTransfer=new e.win.DataTransfer();e.drag.dispatchEvent({type:'dragstart',dataTransfer});assert.equal(dataTransfer.effectAllowed,'move');assert.equal(dataTransfer.getData('application/x-wena-move'),'1');
    const over={type:'dragover',dataTransfer,preventDefault(){this.prevented=true;}};e.target.dispatchEvent(over);assert.equal(over.prevented,true);assert.equal(dataTransfer.dropEffect,'move');
    const drop={type:'drop',dataTransfer,preventDefault(){this.prevented=true;}};e.target.dispatchEvent(drop);await flush();assert.equal(drop.prevented,true);assert.equal(e.live.textContent,'Move complete.');
    e.target.dispatchEvent({type:'drop',dataTransfer:new e.win.DataTransfer()});assert.equal(e.form.style.display,'');assert.equal(e.state.dragging,null);assert.equal(e.live.textContent,'Move failed; use move buttons.');
  });
  await check('unsigned destination rejects binding and restores move controls', () => {
    const e=environment(); e.api.activate(e.doc);e.form.elements=e.form.elements.filter(n=>n.name!=='csrf');assert.equal(e.api.bindDragDrop(e.doc,e.state,100),false);assert.equal(e.form.style.display,'');
  });
  console.log(`${checks} emitted JavaScript runtime integration checks passed`);
})().catch(error=>{console.error(error);process.exitCode=1;});
