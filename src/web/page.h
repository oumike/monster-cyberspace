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
<button onclick="addWifi()">Save network</button>
<div id="wifimsg" class="msg"></div>

<h2>Cyberspace account</h2>
<div id="acct" class="dim"></div>
<label>Email<input id="email" type="email" autocomplete="username"></label>
<label>Password<input id="cpass" type="password" autocomplete="current-password"></label>
<button onclick="login()">Sign in</button>
<button onclick="post('/api/logout',{},'acctmsg')">Sign out</button>
<button onclick="post('/api/resend',{},'acctmsg')">Resend verification</button>
<div class="dim">The device signs in and discards your password. Only a refresh token is kept, and it can't be read back here.</div>
<div id="acctmsg" class="msg"></div>

<h2>Display</h2>
<label>Text size<select id="tsize"><option value="compact">Compact (6×12, 53×20)</option><option value="comfortable">Comfortable (8×16, 40×15)</option></select></label>
<label><input id="buzzer" type="checkbox" style="width:auto"> Buzzer on mentions</label>
<label>Time zone<select id="tzpick" onchange="if(this.value)tz.value=this.value">
<option value="">(type below)</option>
<option>UTC0</option>
<option>EST5EDT,M3.2.0,M11.1.0</option>
<option>CST6CDT,M3.2.0,M11.1.0</option>
<option>MST7MDT,M3.2.0,M11.1.0</option>
<option>PST8PDT,M3.2.0,M11.1.0</option>
<option>GMT0BST,M3.5.0/1,M10.5.0</option>
<option>CET-1CEST,M3.5.0,M10.5.0/3</option>
<option>AEST-10AEDT,M10.1.0,M4.1.0/3</option>
</select></label>
<label>POSIX TZ string<input id="tz" autocomplete="off"></label>
<button onclick="savePrefs()">Save display</button>
<div id="prefmsg" class="msg"></div>

<h2>Security</h2>
<label>Page password (user <b>admin</b>; empty turns it off) <span id="pagestate" class="dim"></span><input id="pagepw" type="password" autocomplete="new-password"></label>
<label>Setup-AP password (WPA2, 8–63 characters; empty makes the AP open) <span id="apstate" class="dim"></span><input id="appw" type="password" autocomplete="new-password"></label>
<button onclick="saveSec()">Save security</button>
<div id="secmsg" class="msg"></div>
<p class="dim">The page is plain HTTP on your LAN, so set a page password on shared networks. The setup AP is open unless a setup-AP password is set, so anyone in radio range can join it while it runs. It runs with no saved Wi-Fi (first boot or after a factory reset), when turned on in the device's Settings, or after the saved networks fail for 5 minutes, and anything entered on it (Wi-Fi password, Cyberspace password) crosses that link unencrypted. Setting a setup-AP password closes that gap for later setups. The refresh token can't be read back through the page.</p>

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
async function load(){
  const s=await call('GET','/api/status');if(s.version===undefined)return;
  const rows=[['firmware','v'+s.version+' on '+s.board],['mode',s.mode==='setup'?'setup (AP '+s.apSsid+')':'station'],
    ['wi-fi',s.ssid?s.ssid+' ('+s.rssiBars+'/4)':'not connected'],['ip',s.ip||'-'],
    ['battery',s.battery<0?'unknown':s.battery+'%'],['chat',conn[s.conn]||'-'],
    ['account',s.signedIn?(s.email||'signed in')+(s.verified?'':' (email not verified)'):'not signed in']];
  $('status').innerHTML='';for(const[k,v]of rows){const tr=$('status').insertRow();tr.insertCell().textContent=k;tr.insertCell().textContent=v;}
  const ul=$('saved');ul.innerHTML='';
  if(!s.saved.length){const li=document.createElement('li');li.textContent='none';li.className='dim';ul.appendChild(li);}
  for(const n of s.saved){const li=document.createElement('li');li.textContent=n+' ';const b=document.createElement('button');b.textContent='remove';
    b.onclick=async()=>{if(!confirm('Remove '+n+'?'))return;show('wifimsg',await call('DELETE','/api/wifi?ssid='+encodeURIComponent(n)));load();};li.appendChild(b);ul.appendChild(li);}
  $('acct').textContent=s.signedIn?'Signed in as '+(s.email||'?'):'Not signed in.';
  if(!$('email').value&&s.email)$('email').value=s.email;
  $('pagestate').textContent=s.pagePasswordSet?'(set)':'(off)';$('apstate').textContent=s.apPasswordSet?'(set)':'(open)';
}
async function loadPrefs(){const p=await call('GET','/api/prefs');if(p.tz===undefined)return;
  $('tsize').value=p.textSize;$('buzzer').checked=p.buzzer;$('tz').value=p.tz;
  $('tzpick').value=[...$('tzpick').options].some(o=>o.value===p.tz)?p.tz:'';}
async function scan(){$('scanmsg').textContent='scanning…';const r=await call('GET','/api/wifi/scan');
  const sel=$('ssidpick');sel.innerHTML='<option value="">pick a network…</option>';
  for(const n of r.networks||[]){const o=document.createElement('option');o.value=o.textContent=n;sel.appendChild(o);}
  sel.hidden=false;$('scanmsg').textContent=(r.networks||[]).length+' found';}
async function addWifi(){const r=await post('/api/wifi',{ssid:$('ssid').value,password:$('wpass').value},'wifimsg');if(r.ok)$('wpass').value='';}
async function login(){$('acctmsg').textContent='Signing in…';$('acctmsg').className='msg dim';
  const r=await call('POST','/api/login',{email:$('email').value,password:$('cpass').value});$('cpass').value='';show('acctmsg',r);load();}
function savePrefs(){post('/api/prefs',{textSize:$('tsize').value,buzzer:$('buzzer').checked,tz:$('tz').value},'prefmsg');}
async function saveSec(){const b={pagePassword:$('pagepw').value,apPassword:$('appw').value};
  if(!b.pagePassword&&!confirm('Leave the page password empty (no password)?'))return;
  const r=await post('/api/security',b,'secmsg');if(r.ok){$('pagepw').value='';$('appw').value='';}}
function reset(){if(confirm('Erase Wi-Fi, account and all settings, then restart into setup mode?')&&confirm('Really erase everything?'))post('/api/factory-reset',{confirm:true},'devmsg');}
load();loadPrefs();setInterval(load,10000);
</script></body></html>)HTML";

}  // namespace web
