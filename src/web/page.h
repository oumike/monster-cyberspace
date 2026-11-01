#pragma once

// The web config page: inline HTML, CSS and JS with no external requests.
namespace web {

// Shared by the config page and the send page (/send, board::webCompose).
#define RC_PAGE_CSS R"CSS(body{background:#000;color:#3f3;font:15px/1.45 ui-monospace,Menlo,Consolas,monospace;margin:0 auto;max-width:44em;padding:1em}
h1{font-size:1.3em;margin:.2em 0 .8em}h2{font-size:1.05em;border-bottom:1px solid #1a5;padding-bottom:.2em;margin-top:1.6em}
input,select,button{font:inherit;background:#020;color:#3f3;border:1px solid #2a6;padding:.35em .5em;border-radius:3px}
textarea{font:inherit;background:#020;color:#3f3;border:1px solid #2a6;padding:.35em .5em;border-radius:3px;width:100%;box-sizing:border-box;margin:.2em 0 .5em}
input,select{width:100%;box-sizing:border-box;margin:.2em 0 .5em}
button{cursor:pointer;margin:.2em .4em .2em 0}button:hover{background:#063}
button.danger{color:#f66;border-color:#a33}
.dim{color:#7a7}.err{color:#f66}.ok{color:#9f9}
table{border-collapse:collapse;width:100%}td{padding:.15em .4em .15em 0;vertical-align:top}td:first-child{color:#7a7;width:9em}
ul{padding-left:1.2em;margin:.3em 0}li button{padding:.1em .4em;font-size:.85em}
.msg{min-height:1.4em}
a{color:#6cf}
)CSS"

// Web compose (board::webCompose): hidden until /api/compose answers.
#define RC_SEND_HTML R"HTML(<div id="sendblock" hidden>
<h2>Send</h2>
<div class="dim">The device sends this as if typed on it.</div>
<label>To<select id="skind" onchange="sendKind()"><option value="open">What the device has open</option><option value="mail">A new C-Mail</option><option value="thread">A new guild thread</option></select></label>
<div id="sopen" class="dim"></div>
<label id="storow" hidden>Recipient<input id="sto" autocomplete="off" placeholder="@name"></label>
<label id="sguildrow" hidden>Guild (its short name)<input id="sguild" autocomplete="off"></label>
<label id="stitlerow" hidden>Title (optional)<input id="stitle" autocomplete="off"></label>
<textarea id="stext" rows="4"></textarea>
<button onclick="sendNow()">Send</button>
<div id="sendmsg" class="msg"></div>
</div>
)HTML"

// Uses $, call and show from the page's script.
#define RC_SEND_JS R"JS(
let sendTimer=0;
async function loadSend(){const r=await call('GET','/api/compose');if(r.label===undefined)return;
  $('sendblock').hidden=false;
  $('sopen').textContent=r.label?(r.takesText?'Open: '+r.label:r.label+' (busy)'):'Nothing open takes text: open a room, a C-Mail or a post on the device.';
  clearTimeout(sendTimer);sendTimer=setTimeout(loadSend,5000);}
function sendKind(){const k=$('skind').value;$('sopen').hidden=k!=='open';$('storow').hidden=k!=='mail';
  $('sguildrow').hidden=$('stitlerow').hidden=k!=='thread';}
async function sendNow(){const k=$('skind').value,b={kind:k,text:$('stext').value};
  if(k==='mail')b.to=$('sto').value;if(k==='thread'){b.guild=$('sguild').value;b.title=$('stitle').value;}
  const r=await call('POST','/api/compose',b);show('sendmsg',r);if(r.ok){$('stext').value='';$('stitle').value='';}loadSend();}
)JS"

static const char kSendPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monster for Cyberspace</title>
<style>)HTML" RC_PAGE_CSS R"HTML(</style></head><body>
<h1>Monster for Cyberspace</h1>
)HTML" RC_SEND_HTML R"HTML(
<p><a href="/config">Settings</a> <span class="dim">(asks for the page password)</span></p>
<script>
const $=id=>document.getElementById(id);
function show(id,r){const e=$(id);e.textContent=r.message||(r.ok?'Done':'Failed');e.className='msg '+(r.ok?'ok':'err');}
async function call(method,url,body){
  try{const r=await fetch(url,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});
    let j={};try{j=await r.json();}catch(e){j={ok:r.ok};}if(j.ok===undefined)j.ok=r.ok;return j;}
  catch(e){return {ok:false,message:'No response from the device'};}
}
)HTML" RC_SEND_JS R"HTML(
loadSend();
</script></body></html>)HTML";

static const char kPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monster for Cyberspace</title>
<style>)HTML" RC_PAGE_CSS R"HTML(</style></head><body>
<h1>Monster for Cyberspace</h1>
)HTML" RC_SEND_HTML R"HTML(

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
<label>Text size<select id="tsize"><option value="compact">Compact (6×12, 53×20)</option><option value="comfortable">Comfortable (8×16, 40×15)</option><option value="large">Large (10×18, 32×13)</option></select></label>
<label id="themerow" hidden>Theme<select id="theme"><option value="default">Default (terminal green)</option><option value="bw">Black &amp; white (white on black)</option><option value="wb">White &amp; black (black on white)</option><optgroup label="cyberspace.online"><option value="dark">Dark</option><option value="light">Light</option><option value="lcd">LCD</option><option value="c64">C64</option><option value="vt320">VT320</option><option value="matrix">Matrix</option><option value="poetry">Poetry</option><option value="brutalist">Brutalist</option><option value="grid">GRiD</option><option value="crypt">Crypt</option><option value="bubblegum">Bubblegum</option><option value="top8">Top8</option></optgroup><optgroup label="Dark takes"><option value="bubblegumdark">Bubblegum Dark</option><option value="top8dark">Top8 Dark</option></optgroup></select></label>
<label id="rotrow" hidden>Orientation<select id="rot"><option value="0">Landscape</option><option value="2">Landscape 180</option><option value="1">Portrait</option><option value="3">Portrait 180</option></select></label>
<label>Screen timeout<select id="scrsecs"><option value="15">15 s</option><option value="30">30 s</option><option value="60">1 min</option><option value="120">2 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="1800">30 min</option><option value="3600">60 min</option><option value="0">Never</option></select></label>
<label>At timeout<select id="scrsaver"><option value="1">Screensaver</option><option value="0">Screen off</option></select></label>
<label id="kbrow" hidden>Keyboard light<select id="kblight"><option value="low">Low</option><option value="medium">Medium</option><option value="high">High</option><option value="off">Off</option></select></label>
<label id="ledrow" hidden>Notify light<select id="ledcolor"><option value="red">Red</option><option value="green">Green</option><option value="blue">Blue</option><option value="yellow">Yellow</option><option value="cyan">Cyan</option><option value="magenta">Magenta</option><option value="white">White</option><option value="off">Off</option></select></label>
<label id="ledtrow" hidden>Light timeout<select id="ledtime"><option value="15">15 s</option><option value="30">30 s</option><option value="60">1 min</option><option value="300">5 min</option><option value="0">Until read</option></select></label>
<label><input id="buzzer" type="checkbox" style="width:auto"> Buzzer on mentions</label>
<label id="spellrow"><input id="spell" type="checkbox" style="width:auto"> Spell check while typing</label>
<button onclick="savePrefs()">Save display</button>
<div id="prefmsg" class="msg"></div>

<div id="phraseblock" hidden>
<h2>Canned replies</h2>
<div class="dim">This device has no keyboard: replying picks one of these (up to 10, 80 characters each), or an emoji. One per line.</div>
<textarea id="phrases" rows="10"></textarea>
<button onclick="savePhrases()">Save replies</button>
<div id="phrasemsg" class="msg"></div>
</div>

<h2>Mascot</h2>
<div class="dim">The monster on the splash and the screensaver: up to 6 lines of 13 plain ASCII characters, and up to 10 speech-bubble phrases of 80 characters, one per line.</div>
<label>Preset<select id="mpreset" onchange="mascotPreset()"></select></label>
<textarea id="mart" cols="13" rows="6" wrap="off" spellcheck="false" style="width:auto;white-space:pre;overflow-x:auto"></textarea>
<textarea id="mphrases" rows="10" spellcheck="false"></textarea>
<button onclick="saveMascot()">Save mascot</button>
<button onclick="resetMascot()">Reset to Fluffy</button>
<div id="mascotmsg" class="msg"></div>

<div id="powerblock" hidden>
<h2>Power</h2>
<div class="dim">On battery the device sleeps when left alone, and wakes now and then to fetch what's new. On USB it stays awake.</div>
<label>Sleep after<select id="idle"><option value="60">1 min</option><option value="120">2 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="1800">30 min</option></select></label>
<label>Wake every<select id="wake"><option value="5">5 min</option><option value="15">15 min</option><option value="30">30 min</option><option value="60">1 h</option><option value="120">2 h</option><option value="0">Never (a button wakes it)</option></select></label>
<button onclick="post('/api/prefs',{idleSecs:+$('idle').value,wakeMins:+$('wake').value},'powermsg')">Save power</button>
<div id="powermsg" class="msg"></div>
</div>

<h2>Time</h2>
<label>Time zone<select id="tzpick" onchange="tzCustom()"></select></label>
<label id="tzrow" hidden>POSIX TZ string<input id="tz" autocomplete="off"></label>
<label>NTP server<input id="ntp" autocomplete="off"></label>
<button onclick="saveTime()">Save time</button>
<div id="timemsg" class="msg"></div>

<h2>Security</h2>
<label>Page password (user <b>admin</b>; empty turns it off) <span id="pagestate" class="dim"></span><input id="pagepw" type="password" autocomplete="new-password"></label>
<label>Setup-AP password (WPA2, 8–63 characters; empty makes the AP open) <span id="apstate" class="dim"></span><input id="appw" type="password" autocomplete="new-password"></label>
<label><input id="webcfg" type="checkbox" style="width:auto" onchange="saveWebCfg(this)"> Serve this page on the joined Wi-Fi (the setup AP always serves it)</label>
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

<h2>Firmware</h2>
<div id="otastate" class="dim"></div>
<button onclick="otaCheck()">Check for updates</button>
<button id="otainst" hidden onclick="otaInstall()">Install</button>
<label><input id="otaauto" type="checkbox" style="width:auto" onchange="post('/api/ota/auto',{auto:this.checked},'otamsg')"> Install updates automatically</label>
<div class="dim">Updates come from this project's GitHub releases over HTTPS and are signature-checked before the device boots them. Settings are kept.</div>
<div id="otamsg" class="msg"></div>

<h2>Device</h2>
<button onclick="if(confirm('Restart the device?'))post('/api/restart',{},'devmsg')">Restart</button>
<button class="danger" onclick="reset()">Factory reset…</button>
<div id="devmsg" class="msg"></div>

<script>)HTML" RC_SEND_JS R"HTML(
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
  $('tsize').value=p.textSize;if(p.theme){$('themerow').hidden=false;$('theme').value=p.theme;}$('buzzer').checked=p.buzzer;$('spell').checked=p.spellCheck!==false;$('tz').value=p.tz;
  $('scrsecs').value=String(p.screenOnSecs);$('scrsaver').value=p.screenSaver?'1':'0';
  if(p.rotation!==undefined){$('rotrow').hidden=false;$('rot').value=String(p.rotation);}
  if(p.kbLight){$('kbrow').hidden=false;$('kblight').value=p.kbLight;}
  if(p.notifyLight){$('ledrow').hidden=$('ledtrow').hidden=false;$('ledcolor').value=p.notifyLight;$('ledtime').value=String(p.notifyTimeout);}
  $('webcfg').checked=p.webConfig!==false;
  if(p.phrases){$('phraseblock').hidden=false;$('spellrow').hidden=true;$('phrases').value=p.phrases.join('\n');}
  if(p.idleSecs!==undefined){$('powerblock').hidden=false;$('idle').value=String(p.idleSecs);$('wake').value=String(p.wakeMins);}
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
function savePrefs(){const b={textSize:$('tsize').value,buzzer:$('buzzer').checked,spellCheck:$('spell').checked,
    screenOnSecs:parseInt($('scrsecs').value,10),screenSaver:$('scrsaver').value==='1'};
  if(!$('themerow').hidden)b.theme=$('theme').value;
  if(!$('rotrow').hidden)b.rotation=parseInt($('rot').value,10);
  if(!$('kbrow').hidden)b.kbLight=$('kblight').value;if(!$('ledrow').hidden){b.notifyLight=$('ledcolor').value;b.notifyTimeout=+$('ledtime').value;}post('/api/prefs',b,'prefmsg');}
function savePhrases(){post('/api/phrases',{phrases:$('phrases').value.split('\n')},'phrasemsg').then(loadPrefs);}
let mascotPresets=[];
async function loadMascot(){const m=await call('GET','/api/mascot');if(!m.art)return;
  mascotPresets=(m.presets&&m.presets.presets)||[];const sel=$('mpreset');sel.innerHTML='<option value="">pick a preset\u2026</option>';
  mascotPresets.forEach((p,i)=>{const o=document.createElement('option');o.value=String(i);o.textContent=p.name;sel.appendChild(o);});
  $('mart').value=m.art.join('\n');$('mphrases').value=m.phrases.join('\n');}
function mascotPreset(){const p=mascotPresets[+$('mpreset').value];if($('mpreset').value!==''&&p)$('mart').value=p.art.join('\n');}
const mascotLines=id=>$(id).value.replace(/\r/g,'').split('\n');
async function saveMascot(){const r=await post('/api/mascot',{art:mascotLines('mart'),phrases:mascotLines('mphrases')},'mascotmsg');if(r.ok)loadMascot();}
async function resetMascot(){if(!confirm("Reset the art to Fluffy and the phrases to his defaults?"))return;
  const r=await post('/api/mascot',{reset:true},'mascotmsg');if(r.ok)loadMascot();}
function saveWebCfg(c){if(!c.checked&&!setupMode&&!confirm('Turn web config off? This page stops answering on this network; turn it back on from the device\'s Settings or the setup AP.')){c.checked=true;return;}
  post('/api/prefs',{webConfig:c.checked},'secmsg');}
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
let otaTimer=0;
async function loadOta(){const o=await call('GET','/api/ota');if(o.state===undefined)return;
  const t={idle:'v'+o.version,checking:'checking\u2026','up-to-date':'v'+o.version+', up to date',
    available:o.latest+' available (running v'+o.version+')',
    installing:'installing '+(o.latest||'')+(o.percent>=0?' '+o.percent+'%':'\u2026'),
    done:'installed, restarting\u2026',failed:'v'+o.version+'; update failed: '+o.error}[o.state]||o.state;
  $('otastate').textContent=o.supported?t:'v'+o.version+'; updates need a USB install of a release first';
  $('otainst').hidden=o.state!=='available';$('otainst').textContent='Install '+(o.latest||'');
  $('otaauto').checked=o.auto;
  clearTimeout(otaTimer);if(o.state==='checking'||o.state==='installing')otaTimer=setTimeout(loadOta,1500);}
async function otaCheck(){await post('/api/ota/check',{},'otamsg');loadOta();}
async function otaInstall(){if(!confirm('Install '+$('otainst').textContent.slice(8)+'? The device restarts.'))return;
  await post('/api/ota/install',{},'otamsg');loadOta();}
load();loadZones().then(loadPrefs);loadBackup();loadOta();loadSend();loadMascot();setInterval(load,10000);
</script></body></html>)HTML";

}  // namespace web
