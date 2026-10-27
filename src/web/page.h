#pragma once

// The web config page: inline HTML, CSS and JS with no external requests.
namespace web {

static const char kPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>rhino-cyberspace</title>
<style>
body{background:#000;color:#3f3;font:15px/1.45 ui-monospace,Menlo,Consolas,monospace;margin:0 auto;max-width:44em;padding:1em}
h1{font-size:1.3em;margin:.2em 0 .8em}h2{font-size:1.05em;border-bottom:1px solid #1a5;padding-bottom:.2em;margin-top:1.6em}
input,select,button{font:inherit;background:#020;color:#3f3;border:1px solid #2a6;padding:.35em .5em;border-radius:3px}
input,select{width:100%;box-sizing:border-box;margin:.2em 0 .5em}
button{cursor:pointer;margin:.2em .4em .2em 0}button:hover{background:#063}
button.danger{color:#f66;border-color:#a33}
.dim{color:#7a7}.err{color:#f66}.ok{color:#9f9}
table{border-collapse:collapse;width:100%}td{padding:.15em .4em .15em 0;vertical-align:top}td:first-child{color:#7a7;width:9em}
ul{padding-left:1.2em;margin:.3em 0}li button{padding:.1em .4em;font-size:.85em}
.msg{min-height:1.4em}
a{color:#6cf}
</style></head><body>
<h1>rhino-cyberspace</h1>

<h2>Status</h2>
<table id="status"><tr><td>loading…</td></tr></table>

<h2>Wi-Fi</h2>
<div class="dim">Saved networks (up to 5), tried in order:</div>
<ul id="saved"></ul>
<button onclick="scan()">Scan</button> <span id="scanmsg" class="dim"></span>
<select id="ssidpick" onchange="ssid.value=this.value" hidden></select>
<label>Network name<input id="ssid" autocomplete="off"></label>
<label>Password (empty for an open network)<input id="wpass" type="password" autocomplete="off"></label>
<label id="wtzrow" hidden>Time zone<select id="wtz"></select></label>
<button onclick="addWifi()">Save network</button>
<div id="wifimsg" class="msg"></div>

<h2>Cyberspace account</h2>
<div id="acct" class="dim"></div>
<label>Email<input id="email" type="email" autocomplete="username"></label>
<label>Password<input id="cpass" type="password" autocomplete="current-password"></label>
<button onclick="login()">Sign in</button>
<button onclick="post('/api/logout',{},'acctmsg')">Sign out</button>
<button onclick="post('/api/resend',{},'acctmsg')">Resend verification</button>
<div class="dim">The device signs in and discards your password. Only a refresh token is kept; it is shown nowhere, but a backup (below) contains it.</div>
<div id="acctmsg" class="msg"></div>

<h2>Display</h2>
<label>Text size<select id="tsize"><option value="compact">Compact (6×12, 53×20)</option><option value="comfortable">Comfortable (8×16, 40×15)</option></select></label>
<label>Theme<select id="theme"><option value="default">Default (terminal green)</option><option value="bw">Black &amp; white (white on black)</option><option value="wb">White &amp; black (black on white)</option></select></label>
<label id="rotrow" hidden>Orientation<select id="rot"><option value="0">Landscape</option><option value="2">Landscape 180</option><option value="1">Portrait</option><option value="3">Portrait 180</option></select></label>
<label><input id="buzzer" type="checkbox" style="width:auto"> Buzzer on mentions</label>
<button onclick="savePrefs()">Save display</button>
<div id="prefmsg" class="msg"></div>

<h2>Time</h2>
<label>Time zone<select id="tzpick" onchange="tzCustom()"></select></label>
<label id="tzrow" hidden>POSIX TZ string<input id="tz" autocomplete="off"></label>
<label>NTP server<input id="ntp" autocomplete="off"></label>
<button onclick="saveTime()">Save time</button>
<div id="timemsg" class="msg"></div>

<h2>Security</h2>
<label>Page password (user <b>admin</b>; empty turns it off) <span id="pagestate" class="dim"></span><input id="pagepw" type="password" autocomplete="new-password"></label>
<label>Setup-AP password (WPA2, 8–63 characters; empty makes the AP open) <span id="apstate" class="dim"></span><input id="appw" type="password" autocomplete="new-password"></label>
<button onclick="saveSec()">Save security</button>
<div id="secmsg" class="msg"></div>
<p class="dim">The page is plain HTTP on your LAN, so set a page password on shared networks. The setup AP is open unless a setup-AP password is set, so anyone in radio range can join it while it runs. It runs with no saved Wi-Fi (first boot or after a factory reset), when turned on in the device's Settings, or after the saved networks fail for 5 minutes, and anything entered on it (Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a setup-AP password closes that gap for later setups. A backup download contains the refresh token and every password, so anyone who can open this page can take them: set a page password.</p>

<h2>Backup</h2>
<p class="dim">A backup holds your Wi-Fi passwords, the page and setup-AP passwords and a Cyberspace sign-in token. Anyone with the file can use your account: keep it private. It travels over plain HTTP unless you use the setup AP with a password.</p>
<p><a href="/api/backup">Download backup</a></p>
<label>Restore from file…<input id="restorefile" type="file" accept=".yaml" onchange="restoreFile()"></label>
<div id="sdblock" hidden>
<button onclick="saveSd()">Save backup to SD card</button>
<ul id="sdfiles"></ul>
</div>
<div id="backupmsg" class="msg"></div>

<h2>Device</h2>
<button onclick="if(confirm('Restart the device?'))post('/api/restart',{},'devmsg')">Restart</button>
<button class="danger" onclick="reset()">Factory reset…</button>
<div id="devmsg" class="msg"></div>

<script>
const $=id=>document.getElementById(id);
function show(id,r){const e=$(id);e.textContent=r.message||(r.ok?'Done':'Failed');e.className='msg '+(r.ok?'ok':'err');}
async function call(method,url,body){
  try{const r=await fetch(url,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});
    let j={};try{j=await r.json();}catch(e){j={ok:r.ok};}if(j.ok===undefined)j.ok=r.ok;return j;}
  catch(e){return {ok:false,message:'No response from the device'};}
}
async function post(url,body,msg){const r=await call('POST',url,body);show(msg,r);setTimeout(load,800);return r;}
const conn=['offline','reconnecting','live'];
let setupMode=false,zones=[],savedTzName=null;
async function load(){
  const s=await call('GET','/api/status');if(s.version===undefined)return;
  setupMode=s.mode==='setup';$('wtzrow').hidden=!setupMode;
  const rows=[['firmware','v'+s.version+' on '+s.board],['mode',s.mode==='setup'?'setup (AP '+s.apSsid+')':'station'],
    ['wi-fi',s.ssid?s.ssid+' ('+s.rssiBars+'/4)':'not connected'],['ip',s.ip||'-'],
    ['battery',s.battery<0?'unknown':s.battery+'%'],['chat',conn[s.conn]||'-'],
    ['account',s.signedIn?(s.email||'signed in')+(s.verified?'':' (email not verified)'):'not signed in'],
    ['gps',s.gps||'-'],['time',s.time||'-'],['zone',s.zone||'-']];
  $('status').innerHTML='';for(const[k,v]of rows){const tr=$('status').insertRow();tr.insertCell().textContent=k;tr.insertCell().textContent=v;}
  const ul=$('saved');ul.innerHTML='';
  if(!s.saved.length){const li=document.createElement('li');li.textContent='none';li.className='dim';ul.appendChild(li);}
  for(const n of s.saved){const li=document.createElement('li');li.textContent=n+' ';const b=document.createElement('button');b.textContent='remove';
    b.onclick=async()=>{if(!confirm('Remove '+n+'?'))return;show('wifimsg',await call('DELETE','/api/wifi?ssid='+encodeURIComponent(n)));load();};li.appendChild(b);ul.appendChild(li);}
  $('acct').textContent=s.signedIn?'Signed in as '+(s.email||'?'):'Not signed in.';
  if(!$('email').value&&s.email)$('email').value=s.email;
  $('pagestate').textContent=s.pagePasswordSet?'(set)':'(off)';$('apstate').textContent=s.apPasswordSet?'(set)':'(open)';
}
const listed=n=>zones.some(z=>z.iana===n);
// Browsers may report an older name for a listed zone.
const tzAlias={'Asia/Calcutta':'Asia/Kolkata','Asia/Saigon':'Asia/Ho_Chi_Minh','Asia/Katmandu':'Asia/Kathmandu',
  'Asia/Rangoon':'Asia/Yangon','Europe/Kiev':'Europe/Kyiv','America/Buenos_Aires':'America/Argentina/Buenos_Aires',
  'Pacific/Samoa':'Pacific/Pago_Pago','UTC':'Etc/UTC','Etc/GMT':'Etc/UTC','Etc/UCT':'Etc/UTC','GMT':'Etc/UTC'};
function browserZone(){try{const n=Intl.DateTimeFormat().resolvedOptions().timeZone;return tzAlias[n]||n;}catch(e){return '';}}
function fillZones(sel,custom){sel.innerHTML='';
  for(const z of zones){const o=document.createElement('option');o.value=z.iana;o.textContent=z.label+' \u2014 '+z.iana;sel.appendChild(o);}
  if(custom){const o=document.createElement('option');o.value='';o.textContent='Custom (POSIX)';sel.appendChild(o);}}
async function loadZones(){const r=await call('GET','/api/zones');zones=Array.isArray(r)?r:[];fillZones($('tzpick'),true);fillZones($('wtz'),true);}
async function loadPrefs(){const p=await call('GET','/api/prefs');if(p.tz===undefined)return;
  $('tsize').value=p.textSize;if(p.theme)$('theme').value=p.theme;$('buzzer').checked=p.buzzer;$('tz').value=p.tz;
  if(p.rotation!==undefined){$('rotrow').hidden=false;$('rot').value=String(p.rotation);}
  $('tzpick').value=listed(p.tzName)?p.tzName:'';tzCustom();$('ntp').value=p.ntpServer;
  // Setup mode keeps a zone already set; only the factory default takes the browser's.
  savedTzName=p.tzName;const keep=$('wtz').querySelector('option[value=""]');
  if(keep)keep.textContent='Keep current ('+p.tz+')';
  const b=browserZone();$('wtz').value=p.tz==='UTC0'&&listed(b)?b:listed(p.tzName)?p.tzName:p.tz==='UTC0'?'Etc/UTC':'';}
function tzCustom(){const n=$('tzpick').value;$('tzrow').hidden=n!=='';const z=zones.find(z=>z.iana===n);if(z)$('tz').value=z.posix;}
async function scan(){$('scanmsg').textContent='scanning…';const r=await call('GET','/api/wifi/scan');
  const sel=$('ssidpick');sel.innerHTML='<option value="">pick a network…</option>';
  for(const n of r.networks||[]){const o=document.createElement('option');o.value=o.textContent=n;sel.appendChild(o);}
  sel.hidden=false;$('scanmsg').textContent=(r.networks||[]).length+' found';}
async function addWifi(){
  // In setup mode the zone is saved first: saving the network restarts the device.
  if(setupMode&&$('wtz').value&&$('wtz').value!==savedTzName){const z=await call('POST','/api/prefs',{tzName:$('wtz').value});if(!z.ok){show('wifimsg',z);return;}}
  const r=await post('/api/wifi',{ssid:$('ssid').value,password:$('wpass').value},'wifimsg');if(r.ok)$('wpass').value='';}
async function login(){$('acctmsg').textContent='Signing in…';$('acctmsg').className='msg dim';
  const r=await call('POST','/api/login',{email:$('email').value,password:$('cpass').value});$('cpass').value='';show('acctmsg',r);load();}
function savePrefs(){const b={textSize:$('tsize').value,theme:$('theme').value,buzzer:$('buzzer').checked};
  if(!$('rotrow').hidden)b.rotation=parseInt($('rot').value,10);post('/api/prefs',b,'prefmsg');}
function saveTime(){post('/api/prefs',{tzName:$('tzpick').value,tz:$('tz').value,ntpServer:$('ntp').value},'timemsg');}
async function saveSec(){const b={pagePassword:$('pagepw').value,apPassword:$('appw').value};
  if(!b.pagePassword&&!confirm('Leave the page password empty (no password)?'))return;
  const r=await post('/api/security',b,'secmsg');if(r.ok){$('pagepw').value='';$('appw').value='';}}
function reset(){if(confirm('Erase Wi-Fi, account and all settings, then restart into setup mode?')&&confirm('Really erase everything?'))post('/api/factory-reset',{confirm:true},'devmsg');}
async function restoreFile(){
  const f=$('restorefile').files[0];if(!f)return;
  if(f.size>16384){show('backupmsg',{ok:false,message:'Backup is over 16 KB'});$('restorefile').value='';return;}
  if(!confirm('Restore '+f.name+' and restart?')){$('restorefile').value='';return;}
  let j;
  try{const r=await fetch('/api/restore',{method:'POST',headers:{'Content-Type':'application/yaml'},body:await f.text()});
    try{j=await r.json();}catch(e){j={ok:r.ok};}if(j.ok===undefined)j.ok=r.ok;}
  catch(e){j={ok:false,message:'No response from the device'};}
  show('backupmsg',j);$('restorefile').value='';
}
async function saveSd(){const r=await call('POST','/api/backup/sd',{});show('backupmsg',r);loadBackup();}
async function loadBackup(){
  const s=await call('GET','/api/backup/sd');if(s.slot===undefined)return;
  $('sdblock').hidden=!s.slot;if(!s.slot)return;
  const ul=$('sdfiles');ul.innerHTML='';
  if(!s.card){const li=document.createElement('li');li.textContent='No SD card';li.className='dim';ul.appendChild(li);return;}
  if(!(s.files||[]).length){const li=document.createElement('li');li.textContent='No backups yet';li.className='dim';ul.appendChild(li);}
  for(const f of s.files||[]){
    const li=document.createElement('li');const span=document.createElement('span');
    span.textContent=f.name+' ('+f.size+' B) ';li.appendChild(span);
    const rb=document.createElement('button');rb.textContent='restore';
    rb.onclick=async()=>{if(!confirm('Restore '+f.name+' and restart?'))return;show('backupmsg',await call('POST','/api/backup/sd/restore?name='+encodeURIComponent(f.name)));};
    const db=document.createElement('button');db.textContent='download';
    db.onclick=()=>{location.href='/api/backup/sd/download?name='+encodeURIComponent(f.name);};
    const xb=document.createElement('button');xb.textContent='delete';xb.className='danger';
    xb.onclick=async()=>{if(!confirm('Delete '+f.name+'?'))return;show('backupmsg',await call('POST','/api/backup/sd/delete?name='+encodeURIComponent(f.name)));loadBackup();};
    li.appendChild(rb);li.appendChild(db);li.appendChild(xb);ul.appendChild(li);
  }
}
load();loadZones().then(loadPrefs);loadBackup();setInterval(load,10000);
</script></body></html>)HTML";

}  // namespace web
