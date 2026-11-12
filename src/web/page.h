#pragma once

#include "feature_flags.h"

// The web config page: inline HTML, CSS and JS with no external requests (the
// setup AP has no internet). It wears the device's theme: /api/prefs sends
// the palettes and the page sets its colour tokens from them. The look is
// the device's own screen: a status bar on top, the Settings list as an
// index, and the hint line as the one Save bar at the bottom.
namespace web {

// Shared by the config page and the send page (/send, board::webCompose).
// The defaults are the device's Default theme.
#define RC_PAGE_CSS R"CSS(:root{--bg:#000;--fg:#b8c0cc;--dim:#555;--bar:#1b2a1b;--barfg:#7cfc7c;--hint:#141414;--hintfg:#666;--sel:#7cfc7c;--selfg:#000;--input:#0d0d0d;--err:#f55;
--soft:color-mix(in srgb,var(--fg) 68%,var(--bg));--line:color-mix(in srgb,var(--fg) 22%,var(--bg));color-scheme:dark}
*{box-sizing:border-box}
[hidden]{display:none!important}
html{background:var(--bg)}
body{margin:0;background:var(--bg);color:var(--fg);font:15px/1.5 ui-monospace,"SF Mono","Cascadia Mono",Menlo,Consolas,"DejaVu Sans Mono",monospace;-webkit-text-size-adjust:100%}
.top{position:sticky;top:0;z-index:3;display:flex;justify-content:space-between;gap:2ch;padding:.4em 2ch;background:var(--bar);color:var(--barfg);white-space:nowrap}
.top span{min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:pre}.top span:first-child{flex-shrink:0}
.wrap{display:grid;grid-template-columns:20ch minmax(0,66ch);gap:0 4ch;justify-content:center;padding:1.75em 2ch 7em}
.wrap.single{grid-template-columns:minmax(0,66ch)}
nav{position:sticky;top:3.5em;align-self:start}
nav a{display:block;padding:.1em 1ch;color:var(--fg);text-decoration:none;border-radius:2px}
nav a:hover{background:var(--line)}
nav a.on{background:var(--sel);color:var(--selfg)}
@media(max-width:860px){.wrap{grid-template-columns:minmax(0,1fr)}nav{display:none}}
section{padding-bottom:2em;scroll-margin-top:3.5em}
h2{font-size:1em;font-weight:700;color:var(--barfg);margin:0 0 .6em;padding-bottom:.3em;border-bottom:1px solid var(--line)}
p{color:var(--soft);margin:.3em 0 .8em;max-width:64ch}
.row{display:grid;grid-template-columns:18ch minmax(0,1fr);gap:.25em 2ch;align-items:baseline;padding:.4em 0 .4em 1.5ch;margin-left:calc(-1.5ch - 3px);border-left:3px solid transparent}
.row.dirty{border-left-color:var(--sel)}
.row>:first-child{color:var(--fg)}
.row .note{grid-column:2;color:var(--soft);font-size:.9em}
@media(max-width:560px){.row{grid-template-columns:minmax(0,1fr)}.row .note{grid-column:1}}
.trio{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:1ch}
.trio label{color:var(--soft);font-size:.9em}
.trio.quad{grid-template-columns:repeat(2,minmax(0,1fr))}
.range{display:flex;gap:2ch;align-items:center}.range span{color:var(--soft);white-space:nowrap}
input,select,textarea{font:inherit;color:var(--fg);background:var(--input);border:1px solid var(--line);border-radius:2px;padding:.35em .6ch;width:100%}
input:disabled{opacity:.5}
input[type=checkbox]{width:auto;margin:0;accent-color:var(--sel)}
input[type=range]{padding:0;border:0;background:none;accent-color:var(--sel)}
input[type=file]{padding:.3em}
textarea{resize:vertical;display:block}
.art{width:15ch;white-space:pre;overflow-x:auto}
.check{display:flex;gap:1ch;align-items:baseline;color:var(--fg);margin:.15em 0}
:focus-visible{outline:2px solid var(--sel);outline-offset:1px}
button{font:inherit;color:var(--fg);background:transparent;border:1px solid var(--line);border-radius:2px;padding:.35em 1.4ch;cursor:pointer}
button:hover:not(:disabled){border-color:var(--fg)}
a.btn{display:inline-block;color:var(--fg);text-decoration:none;border:1px solid var(--line);border-radius:2px;padding:.35em 1.4ch}a.btn:hover{border-color:var(--fg)}
textarea.dirty{border-left:3px solid var(--sel)}
button.go{background:var(--sel);color:var(--selfg);border-color:var(--sel);font-weight:700}
button.danger{color:var(--err);border-color:var(--err)}
button:disabled{opacity:.4;cursor:default}
.acts{display:flex;flex-wrap:wrap;gap:.5em 1ch;margin:.6em 0}
.msg{min-height:1.5em;color:var(--soft)}.msg.ok{color:var(--fg)}.msg.err{color:var(--err)}
dl{display:grid;grid-template-columns:12ch minmax(0,1fr);gap:.15em 2ch;margin:0}dt{color:var(--soft)}dd{margin:0;overflow-wrap:anywhere}
ul.list{list-style:none;padding:0;margin:.3em 0 .8em}
ul.list li{display:flex;flex-wrap:wrap;gap:.4em 1ch;align-items:baseline;padding:.3em 0;border-bottom:1px solid var(--line)}
ul.list li span{flex:1;min-width:12ch;overflow-wrap:anywhere}ul.list li.none{color:var(--soft)}
ul.list button{padding:.1em 1ch;font-size:.9em}
.save{position:fixed;left:0;right:0;bottom:0;z-index:3;display:flex;flex-wrap:wrap;align-items:center;gap:.5em 2ch;padding:.6em 2ch;background:var(--hint);color:var(--hintfg);border-top:1px solid var(--line)}
.save .what{flex:1;min-width:24ch}
@media(max-width:560px){.save .what{flex-basis:100%}}
.save.dirty .what{color:var(--fg)}.save .what.err{color:var(--err)}
a{color:var(--sel)}
@media(prefers-reduced-motion:no-preference){html,body,.top,.save,nav a,input,select,textarea,button,.row{transition:background-color .3s,color .3s,border-color .3s}}
)CSS"

// The send targets each built feature adds (feature_flags.h).
#if RC_FEAT_CMAIL
#define RC_SEND_MAIL_OPT R"HTML(<option value="mail">A new C-Mail</option>)HTML"
#else
#define RC_SEND_MAIL_OPT ""
#endif
#if RC_FEAT_CIRC && !RC_FEAT_CMAIL && !RC_FEAT_FEED
#define RC_SEND_NOTHING_OPEN "open a room"
#elif RC_FEAT_CIRC
#define RC_SEND_NOTHING_OPEN "open a room, a C-Mail or a post"
#else
#define RC_SEND_NOTHING_OPEN "open a C-Mail or a post"
#endif
#if RC_FEAT_GUILDS
#define RC_SEND_THREAD_OPT R"HTML(<option value="thread">A new guild thread</option>)HTML"
#else
#define RC_SEND_THREAD_OPT ""
#endif

// The config page sections each built feature adds (feature_flags.h); the page
// never asks for a missing one. The status bar still shows the version.
#if RC_FEAT_MASCOT
#define RC_MASCOT_HTML R"HTML(<section id="mascot" data-name="Mascot">
<h2>Mascot</h2>
<p>The monster on the splash and the screensaver: up to 6 lines of 13 plain ASCII characters, and up to 10 speech-bubble phrases of 80 characters, one per line.</p>
<div class="row"><label for="mpreset">Start from</label><select id="mpreset"></select></div>
<div class="row"><label for="mart">Art</label><textarea id="mart" class="art" cols="13" rows="6" wrap="off" spellcheck="false"></textarea></div>
<div class="row"><label for="mphrases">Phrases</label><textarea id="mphrases" rows="8" spellcheck="false"></textarea></div>
<div class="acts"><button type="button" id="mreset">Put Fluffy back</button></div>
</section>
)HTML"
#define RC_MASCOT_JS R"JS(let mascotPresets=[],mascotDefault={art:[],phrases:[]};
async function loadMascot(){const m=await call('GET','/api/mascot');if(!m.art)return;
  mascotPresets=(m.presets&&m.presets.presets)||[];mascotDefault={art:m.defaultArt||[],phrases:m.defaultPhrases||[]};
  const sel=$('mpreset');sel.innerHTML='<option value="">Your current art</option>';
  mascotPresets.forEach((p,i)=>{const o=document.createElement('option');o.value=String(i);o.textContent=p.name;sel.appendChild(o);});
  $('mart').value=m.art.join('\n');$('mphrases').value=m.phrases.join('\n');}
$('mpreset').onchange=()=>{const p=mascotPresets[+$('mpreset').value];if($('mpreset').value!==''&&p){$('mart').value=p.art.join('\n');mark();}};
$('mreset').onclick=()=>{$('mart').value=mascotDefault.art.join('\n');$('mphrases').value=mascotDefault.phrases.join('\n');$('mpreset').value='';mark();};
const lines=id=>$(id).value.replace(/\r/g,'').split('\n');
F.push({ids:['mart','mphrases'],label:'Mascot',put:b=>{b.mascot={art:lines('mart'),phrases:lines('mphrases')};}});
loaders.push(loadMascot);
)JS"
#else
#define RC_MASCOT_JS ""
#define RC_MASCOT_HTML ""
#endif
#if RC_FEAT_OTA
#define RC_OTA_HTML R"HTML(<section id="firmware" data-name="Firmware">
<h2>Firmware</h2>
<p id="otastate"></p>
<div class="acts"><button type="button" id="otacheck">Check for updates</button><button type="button" class="go" id="otainst" hidden>Install</button></div>
<div id="otamsg" class="msg"></div>
<div class="row"><span>Updates</span><label class="check"><input id="otaauto" type="checkbox"> Install updates automatically</label>
<span class="note">They come from this project's GitHub releases over HTTPS and are signature-checked before the device boots them. Settings are kept.</span></div>
</section>
)HTML"
#define RC_OTA_JS R"JS(let otaTimer=0;
async function loadOta(){const o=await call('GET','/api/ota');if(o.state===undefined)return;
  const t={idle:'Running v'+o.version+'.',checking:'Checking…','up-to-date':'Running v'+o.version+', the latest.',
    available:o.latest+' is available (running v'+o.version+').',
    installing:'Installing '+(o.latest||'')+(o.percent>=0?': '+o.percent+'%':'…'),
    done:'Installed. Restarting…',failed:'Running v'+o.version+'. The update failed: '+o.error}[o.state]||o.state;
  $('otastate').textContent=o.supported?t:'Running v'+o.version+'. Updates need a USB install of a release first.';
  $('otainst').hidden=o.state!=='available';$('otainst').textContent='Install '+(o.latest||'');
  clearTimeout(otaTimer);if(o.state==='checking'||o.state==='installing')otaTimer=setTimeout(loadOta,1500);
  return o;}
async function loadOtaAuto(){const o=await loadOta();if(o)$('otaauto').checked=o.auto;}
$('otacheck').onclick=async()=>{show('otamsg',await call('POST','/api/ota/check',{}));loadOta();};
$('otainst').onclick=async()=>{if(!confirm($('otainst').textContent+'? The device restarts.'))return;
  show('otamsg',await call('POST','/api/ota/install',{}));loadOta();};
F.push({ids:['otaauto'],label:'Install updates automatically',put:b=>{b.autoUpdate=$('otaauto').checked;}});
loaders.push(loadOtaAuto);
)JS"
#else
#define RC_OTA_JS ""
#define RC_OTA_HTML ""
#endif

// Web compose (board::webCompose): hidden until /api/compose answers.
#define RC_SEND_HTML R"HTML(<section id="sendblock" data-name="Send" hidden>
<h2>Send</h2>
<p>The device sends this as if typed on it.</p>
<div class="row"><label for="skind">To</label><select id="skind"><option value="open">What the device has open</option>)HTML" RC_SEND_MAIL_OPT RC_SEND_THREAD_OPT R"HTML(</select>
<span class="note" id="sopen"></span></div>
<div class="row" id="storow" hidden><label for="sto">Recipient</label><input id="sto" autocomplete="off" placeholder="@name"></div>
<div class="row" id="sguildrow" hidden><label for="sguild">Guild</label><input id="sguild" autocomplete="off" placeholder="its short name"></div>
<div class="row" id="stitlerow" hidden><label for="stitle">Title</label><input id="stitle" autocomplete="off" placeholder="optional"></div>
<div class="row"><label for="stext">Text</label><textarea id="stext" rows="4"></textarea></div>
<div class="acts"><button type="button" class="go" id="sendnow">Send</button></div>
<div id="sendmsg" class="msg"></div>
</section>
)HTML"

// Uses $, call and show from the page's script.
#define RC_SEND_JS R"JS(
let sendTimer=0;
async function loadSend(){const r=await call('GET','/api/compose');if(r.label===undefined)return;
  if($('sendblock').hidden){$('sendblock').hidden=false;if(window.buildNav)buildNav();}
  $('sopen').textContent=r.label?(r.takesText?'Open: '+r.label:r.label+' (busy)'):'Nothing open takes text: )JS" RC_SEND_NOTHING_OPEN R"JS( on the device.';
  clearTimeout(sendTimer);sendTimer=setTimeout(loadSend,5000);}
$('skind').onchange=()=>{const k=$('skind').value;$('sopen').hidden=k!=='open';$('storow').hidden=k!=='mail';
  $('sguildrow').hidden=$('stitlerow').hidden=k!=='thread';};
$('sendnow').onclick=async()=>{const k=$('skind').value,b={kind:k,text:$('stext').value};
  if(k==='mail')b.to=$('sto').value;if(k==='thread'){b.guild=$('sguild').value;b.title=$('stitle').value;}
  const r=await call('POST','/api/compose',b);show('sendmsg',r);if(r.ok){$('stext').value='';$('stitle').value='';}loadSend();};
)JS"

// $, show and call, shared by both pages.
#define RC_BASE_JS R"JS(
const $=id=>document.getElementById(id);
function show(id,r){const e=$(id);e.textContent=r.message||(r.ok?'Done':'Failed');e.className='msg '+(r.ok?'ok':'err');}
async function call(method,url,body){
  try{const r=await fetch(url,{method,headers:{'Content-Type':'application/json'},body:body?JSON.stringify(body):undefined});
    let j={};try{j=await r.json();}catch(e){j={ok:r.ok};}if(j.ok===undefined)j.ok=r.ok;return j;}
  catch(e){return {ok:false,message:'No response from the device. Check it is on and on this network.'};}
}
)JS"

static const char kSendPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monster for Cyberspace</title>
<style>)HTML" RC_PAGE_CSS R"HTML(</style></head><body>
<header class="top"><span><b>monster</b> send</span><span><a href="/config" style="color:inherit">settings</a></span></header>
<div class="wrap single"><main>
)HTML" RC_SEND_HTML R"HTML(
<p>Settings ask for the page password.</p>
</main></div>
<script>)HTML" RC_BASE_JS RC_SEND_JS R"HTML(
loadSend();
</script></body></html>)HTML";

static const char kPage[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Monster for Cyberspace</title>
<style>)HTML" RC_PAGE_CSS R"HTML(</style></head><body>
<header class="top"><span><b>monster</b> settings</span><span id="topst">connecting&hellip;</span></header>
<div class="wrap">
<nav id="nav" aria-label="Sections"></nav>
<main>
)HTML" RC_SEND_HTML R"HTML(
<section id="status" data-name="Status">
<h2>Status</h2>
<dl id="statuslist"><dt>device</dt><dd>loading&hellip;</dd></dl>
</section>

<section id="wifi" data-name="Wi-Fi">
<h2>Wi-Fi</h2>
<p>Saved networks, up to 5, tried in this order.</p>
<ul class="list" id="saved"></ul>
<div class="row"><label for="ssid">Network</label><div><input id="ssid" autocomplete="off">
<div class="acts"><button type="button" id="scanbtn">Scan for networks</button><select id="ssidpick" hidden></select></div></div></div>
<div class="row"><label for="wpass">Password</label><input id="wpass" type="password" autocomplete="off" placeholder="empty for an open network"></div>
<div class="row" id="wtzrow" hidden><label for="wtz">Time zone</label><select id="wtz"></select></div>
<div class="acts"><button type="button" id="addwifi">Add network</button></div>
<div id="wifimsg" class="msg"></div>
</section>

<section id="account" data-name="Account">
<h2>Cyberspace account</h2>
<p id="acct"></p>
<div class="row"><label for="email">Email</label><input id="email" type="email" autocomplete="username"></div>
<div class="row"><label for="cpass">Password</label><input id="cpass" type="password" autocomplete="current-password"></div>
<div class="acts"><button type="button" class="go" id="signin">Sign in</button><button type="button" id="signout">Sign out</button><button type="button" id="resend">Resend verification email</button></div>
<div id="acctmsg" class="msg"></div>
<p>The device signs in and forgets your password. It keeps only a sign-in token, shown nowhere; a backup contains it.</p>
</section>

<section id="display" data-name="Screen">
<h2>Screen</h2>
<div class="row" id="themerow" hidden><label for="theme">Theme</label><select id="theme"></select><span class="note">This page wears it too.</span></div>
<div class="row" id="tsizerow" hidden><label for="tsize">Text size</label><div class="range"><input id="tsize" type="range" min="0" max="2" step="1"><span id="tsizeval"></span></div></div>
<div class="row" id="chatrow" hidden><label for="chat">Chat style</label><select id="chat"><option value="irc">IRC</option><option value="bubbles">Bubbles</option></select></div>
<div class="row" id="unrrow" hidden><label for="unr">Unread shows</label><select id="unr"><option value="all">All</option><option value="mentions">Replies &amp; mentions</option></select></div>
<div class="row" id="rotrow" hidden><label for="rot">Orientation</label><select id="rot"><option value="0">Landscape</option><option value="2">Landscape 180</option><option value="1">Portrait</option><option value="3">Portrait 180</option></select></div>
<div class="row" id="brightrow" hidden><label for="bright">Brightness</label><div class="range"><input id="bright" type="range" min="10" max="100" step="10"><span id="brightval"></span></div></div>
<div class="row"><label for="scrsecs">Screen timeout</label><select id="scrsecs"><option value="15">15 s</option><option value="30">30 s</option><option value="60">1 min</option><option value="120">2 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="1800">30 min</option><option value="3600">60 min</option><option value="0">Never</option></select></div>
<div class="row"><label for="scrsaver">At timeout</label><select id="scrsaver"><option value="1">Show the screensaver</option><option value="0">Turn the screen off</option></select></div>
<div class="row" id="spellrow" hidden><span>Typing</span><label class="check"><input id="spell" type="checkbox"> Spell check while typing</label></div>
</section>

<section id="lights" data-name="Lights and sound">
<h2>Lights and sound</h2>
<div class="row" id="volrow" hidden><label for="vol">Alert sound</label><div class="range"><input id="vol" type="range" min="0" max="100" step="10"><span id="volval"></span></div><span class="note">For mentions, C-Mail and Feed.</span></div>
<div class="row" id="buzzrow" hidden><span>Alert sound</span><label class="check"><input id="buzzer" type="checkbox"> Beep on mentions, C-Mail and Feed</label></div>
<div class="row" id="vibrow" hidden><span>Vibration</span><div><label class="check"><input id="tapvib" type="checkbox"> On every tap</label><label class="check"><input id="alertvib" type="checkbox"> On alerts</label></div></div>
<div class="row" id="ledrow" hidden><span>Notify light</span><div class="trio quad"><label>channel<select id="ledcolor" class="colour"></select></label><label id="ledmentl">mentions<select id="ledment" class="colour"></select></label><label id="ledfeedl"><span id="ledfeedn">Feed</span><select id="ledfeed" class="colour"></select></label><label id="ledcmaill"><span id="ledcmailn">C-Mail</span><select id="ledcmail" class="colour"></select></label></div><span class="note">Channel is the last room opened. Each pending one blinks in turn.</span></div>
<div class="row" id="ledtrow" hidden><label for="ledtime">Light stays on</label><select id="ledtime"><option value="15">15 s</option><option value="30">30 s</option><option value="60">1 min</option><option value="300">5 min</option><option value="0">Until read</option></select></div>
<div class="row" id="ledlvlrow" hidden><label for="ledlvl">LED brightness</label><div class="range"><input id="ledlvl" type="range" min="0" max="4" step="1"><span id="ledlvlval"></span></div></div>
<div class="row" id="pwrrow" hidden><label for="pwrlight">Power light</label><select id="pwrlight" class="colour"></select></div>
<div class="row" id="wifirow" hidden><span>Wi-Fi light</span><div class="trio"><label>joined<select id="wifijoin" class="colour"></select></label><label>setup AP<select id="wifiap" class="colour"></select></label><label>looking<select id="wifilook" class="colour"></select></label></div></div>
<div class="row" id="kbrow" hidden><label for="kblight">Keyboard light</label><select id="kblight"><option value="low">Low</option><option value="medium">Medium</option><option value="high">High</option><option value="off">Off</option></select></div>
</section>

<section id="phraseblock" data-name="Canned replies" hidden>
<h2>Canned replies</h2>
<p>This device has no keyboard: replying picks one of these, or an emoji. Up to 10, 80 characters each, one per line.</p>
<textarea id="phrases" rows="10" aria-label="Canned replies"></textarea>
</section>

)HTML" RC_MASCOT_HTML R"HTML(
<section id="powerblock" data-name="Power" hidden>
<h2>Power</h2>
<p>On battery the device sleeps when left alone, and wakes now and then to fetch what's new. On USB it stays awake.</p>
<div class="row"><label for="idle">Sleep after</label><select id="idle"><option value="60">1 min</option><option value="120">2 min</option><option value="300">5 min</option><option value="600">10 min</option><option value="1800">30 min</option></select></div>
<div class="row"><label for="wake">Wake every</label><select id="wake"><option value="5">5 min</option><option value="15">15 min</option><option value="30">30 min</option><option value="60">1 h</option><option value="120">2 h</option><option value="0">Never (a button wakes it)</option></select></div>
</section>

<section id="time" data-name="Time">
<h2>Time</h2>
<div class="row"><label for="tzpick">Time zone</label><select id="tzpick"></select></div>
<div class="row" id="tzrow" hidden><label for="tz">POSIX TZ</label><input id="tz" autocomplete="off" spellcheck="false"></div>
<div class="row"><label for="ntp">NTP server</label><input id="ntp" autocomplete="off" spellcheck="false"></div>
</section>

<section id="security" data-name="Security">
<h2>Security</h2>
<div class="row"><label for="pagepw">Page password</label><input id="pagepw" type="password" autocomplete="new-password">
<span class="note"><span id="pagestate"></span> User name <b>admin</b>.</span>
<label class="check note" id="pageoffrow" hidden><input id="pageoff" type="checkbox"> Turn the page password off</label></div>
<div class="row"><label for="appw">Setup-AP password</label><input id="appw" type="password" autocomplete="new-password">
<span class="note"><span id="apstate"></span> WPA2, 8 to 63 characters. It applies the next time setup mode runs.</span>
<label class="check note" id="apopenrow" hidden><input id="apopen" type="checkbox"> Make the setup AP open</label></div>
<div class="row"><span>This page</span><label class="check"><input id="webcfg" type="checkbox"> Serve it on the joined Wi-Fi</label><span class="note">The setup AP always serves it.</span></div>
<p>This page is plain HTTP on your network, so set a page password on shared networks. The setup AP is open unless it has a password, so anyone in radio range can join it while it runs: with no saved Wi-Fi, when turned on in the device's Settings, or after the saved networks fail for 5 minutes. Anything typed on it, Wi-Fi and Cyberspace passwords included, crosses that link unencrypted. A backup holds the sign-in token and every password, so anyone who can open this page can take them.</p>
</section>

<section id="backup" data-name="Backup">
<h2>Backup</h2>
<p>A backup holds your Wi-Fi passwords, the page and setup-AP passwords and a Cyberspace sign-in token. Anyone with the file can use your account: keep it private.</p>
<div class="acts"><a class="btn" href="/api/backup" download>Download backup</a></div>
<div class="row"><label for="restorefile">Restore</label><input id="restorefile" type="file" accept=".yaml"></div>
<div id="sdblock" hidden>
<div class="acts"><button type="button" id="savesd">Save a backup to the SD card</button></div>
<ul class="list" id="sdfiles"></ul>
</div>
<div id="backupmsg" class="msg"></div>
</section>

)HTML" RC_OTA_HTML R"HTML(
<section id="device" data-name="Device">
<h2>Device</h2>
<div class="acts"><button type="button" id="restart">Restart</button><button type="button" class="danger" id="freset">Factory reset</button></div>
<div id="devmsg" class="msg"></div>
</section>
</main>
</div>

<div class="save" id="savebar" role="region" aria-label="Save">
<span class="what" id="savewhat" aria-live="polite">Loading settings&hellip;</span>
<button type="button" id="discard" disabled>Discard</button>
<button type="button" class="go" id="savebtn" disabled>Save changes</button>
</div>

<script>)HTML" RC_BASE_JS RC_SEND_JS R"JS(
const conn=['offline','reconnecting','live'];
const COLOURS=['red','green','blue','yellow','cyan','magenta','white','off'];
const TSIZES=['compact','comfortable','large'],TSIZE_LABELS=['Compact, 6×12','Comfortable, 8×16','Large, 10×18'];
const LEDLVLS=['off','dim','low','medium','high'];
const cap=s=>s[0].toUpperCase()+s.slice(1);
let setupMode=false,zones=[],savedTzName=null,themes=[],palette=null,ready=false,saving=false,saved=null;
document.querySelectorAll('select.colour').forEach(s=>{for(const c of COLOURS){const o=document.createElement('option');o.value=c;o.textContent=cap(c);s.appendChild(o);}});

// ---- the device's colours ----
const TOKENS=['bg','fg','dim','bar','barfg','hint','hintfg','sel','selfg','input','err'];
function paint(c){if(!c||c.length<TOKENS.length)return;const r=document.documentElement.style;
  TOKENS.forEach((t,i)=>r.setProperty('--'+t,c[i]));
  const n=parseInt(c[0].slice(1),16),lum=(n>>16&255)*.3+(n>>8&255)*.59+(n&255)*.11;r.setProperty('color-scheme',lum>128?'light':'dark');}
function paintTheme(){const t=themes.find(t=>t.key===$('theme').value);paint(t?t.c:palette);}
$('theme').onchange=paintTheme;

// ---- the saveable fields: one Save sends every changed one ----
// ids: the controls; put(b): adds the field to the /api/settings body.
const P=b=>b.prefs||(b.prefs={});
const F=[
  {ids:['theme'],label:'Theme',put:b=>{P(b).theme=$('theme').value;}},
  {ids:['tsize'],label:'Text size',put:b=>{P(b).textSize=TSIZES[+$('tsize').value];}},
  {ids:['chat'],label:'Chat style',put:b=>{P(b).chatStyle=$('chat').value;}},
  {ids:['unr'],label:'Unread shows',put:b=>{P(b).unreadFilter=$('unr').value;}},
  {ids:['rot'],label:'Orientation',put:b=>{P(b).rotation=+$('rot').value;}},
  {ids:['bright'],label:'Brightness',put:b=>{P(b).brightness=+$('bright').value;}},
  {ids:['scrsecs'],label:'Screen timeout',put:b=>{P(b).screenOnSecs=+$('scrsecs').value;}},
  {ids:['scrsaver'],label:'At timeout',put:b=>{P(b).screenSaver=$('scrsaver').value==='1';}},
  {ids:['spell'],label:'Spell check',put:b=>{P(b).spellCheck=$('spell').checked;}},
  {ids:['vol'],label:'Alert sound',put:b=>{const v=+$('vol').value;P(b).buzzer=v>0;if(v>0)P(b).volume=v;}},
  {ids:['buzzer'],label:'Alert sound',put:b=>{P(b).buzzer=$('buzzer').checked;}},
  {ids:['tapvib','alertvib'],label:'Vibration',put:b=>{P(b).tapVibrate=$('tapvib').checked;P(b).alertVibrate=$('alertvib').checked;}},
  {ids:['ledcolor','ledment','ledfeed','ledcmail'],label:'Notify light',put:b=>{P(b).notifyLight=$('ledcolor').value;for(const [id,k] of [['ledment','notifyMention'],['ledfeed','notifyFeed'],['ledcmail','notifyCmail']])if(!$(id+'l').hidden)P(b)[k]=$(id).value;}},
  {ids:['ledtime'],label:'Light stays on',put:b=>{P(b).notifyTimeout=+$('ledtime').value;}},
  {ids:['ledlvl'],label:'LED brightness',put:b=>{P(b).ledLevel=LEDLVLS[+$('ledlvl').value];}},
  {ids:['pwrlight'],label:'Power light',put:b=>{P(b).powerLight=$('pwrlight').value;}},
  {ids:['wifijoin','wifiap','wifilook'],label:'Wi-Fi light',put:b=>{P(b).wifiJoined=$('wifijoin').value;P(b).wifiSetup=$('wifiap').value;P(b).wifiLooking=$('wifilook').value;}},
  {ids:['kblight'],label:'Keyboard light',put:b=>{P(b).kbLight=$('kblight').value;}},
  {ids:['phrases'],label:'Canned replies',put:b=>{b.phrases=$('phrases').value.split('\n');}},
  {ids:['idle'],label:'Sleep after',put:b=>{P(b).idleSecs=+$('idle').value;}},
  {ids:['wake'],label:'Wake every',put:b=>{P(b).wakeMins=+$('wake').value;}},
  {ids:['tzpick','tz'],label:'Time zone',put:b=>{P(b).tzName=$('tzpick').value;P(b).tz=$('tz').value;}},
  {ids:['ntp'],label:'NTP server',put:b=>{P(b).ntpServer=$('ntp').value;}},
  {ids:['pagepw','pageoff'],label:'Page password',put:b=>{(b.security=b.security||{}).pagePassword=$('pageoff').checked?'':$('pagepw').value;}},
  {ids:['appw','apopen'],label:'Setup-AP password',put:b=>{(b.security=b.security||{}).apPassword=$('apopen').checked?'':$('appw').value;}},
  {ids:['webcfg'],label:'Serve this page',put:b=>{P(b).webConfig=$('webcfg').checked;}},
];
const loaders=[()=>loadZones().then(loadPrefs)];
const val=f=>f.ids.map(id=>{const e=$(id);return !e?'':e.type==='checkbox'?e.checked:e.value;}).join('\u0001');
// The save bar shows a notice (a save's result) until the next edit.
let notice=null;
function baseline(){saved=new Map(F.map(f=>[f,val(f)]));ready=true;mark();}
function changed(){return ready?F.filter(f=>f.ids.every(id=>$(id))&&val(f)!==saved.get(f)):[];}
function mark(){
  const ch=changed();
  for(const f of F){const e=$(f.ids[0]);if(e)(e.closest('.row')||e).classList.toggle('dirty',ch.includes(f));}
  document.querySelectorAll('#nav a').forEach(a=>{const s=$(a.dataset.s);a.textContent=s.dataset.name+(ch.some(f=>s.contains($(f.ids[0])))?' *':'');});
  const bar=$('savebar'),what=$('savewhat');bar.classList.toggle('dirty',ch.length>0);
  what.classList.toggle('err',!!(notice&&notice.err));
  if(notice)what.textContent=notice.text;
  else if(ready){const names=[...new Set(ch.map(f=>f.label))];
    what.textContent=names.length?names.length+' unsaved: '+names.join(', '):'No unsaved changes';}
  $('savebtn').disabled=$('discard').disabled=!ch.length||saving;}
const edited=e=>{if(!e.target.closest('main'))return;if(!saving)notice=null;mark();};
document.addEventListener('input',edited);document.addEventListener('change',edited);
$('pageoff').onchange=()=>{$('pagepw').disabled=$('pageoff').checked;mark();};
$('apopen').onchange=()=>{$('appw').disabled=$('apopen').checked;mark();};
function clearSecrets(){for(const id of ['pagepw','appw'])$(id).value='',$(id).disabled=false;$('pageoff').checked=$('apopen').checked=false;}
async function reloadAll(){ready=false;await Promise.all(loaders.map(l=>l()));baseline();}
async function save(){
  const ch=changed();if(!ch.length||saving)return;
  const b={};for(const f of ch)f.put(b);
  if(b.prefs&&b.prefs.webConfig===false&&!setupMode&&!confirm('Stop serving this page on this network? Turn it back on from the device\'s Settings or the setup AP.'))return;
  if(b.security&&b.security.pagePassword===''&&$('pageoff').checked&&!confirm('Turn the page password off? Anyone on this network can then open this page.'))return;
  saving=true;notice={text:'Saving…'};mark();
  const r=await call('POST','/api/settings',b);
  if(!r.ok){saving=false;notice={text:'Nothing was saved. '+(r.message||''),err:true};mark();return;}
  clearSecrets();await reloadAll();saving=false;notice={text:r.message||'Saved'};mark();load();}
$('savebtn').onclick=save;
$('discard').onclick=async()=>{clearSecrets();notice={text:'Discarded'};await reloadAll();};
document.addEventListener('keydown',e=>{if((e.ctrlKey||e.metaKey)&&e.key==='s'){e.preventDefault();save();}});
window.addEventListener('beforeunload',e=>{if(changed().length){e.preventDefault();e.returnValue='';}});

// ---- the section index ----
let spy=null;
function buildNav(){const nav=$('nav');nav.innerHTML='';if(spy)spy.disconnect();
  spy=new IntersectionObserver(es=>{for(const e of es)if(e.isIntersecting)nav.querySelectorAll('a').forEach(a=>a.classList.toggle('on',a.dataset.s===e.target.id));},{rootMargin:'-20% 0px -70% 0px'});
  document.querySelectorAll('main>section').forEach(s=>{if(s.hidden)return;
    const a=document.createElement('a');a.href='#'+s.id;a.dataset.s=s.id;a.textContent=s.dataset.name;nav.appendChild(a);spy.observe(s);});
  mark();}

// ---- status, polled ----
async function load(){
  const s=await call('GET','/api/status');if(s.version===undefined){$('topst').textContent='no answer from the device';return;}
  setupMode=s.mode==='setup';$('wtzrow').hidden=!setupMode;
  const bars=s.ssid?' wi-fi '+s.rssiBars+'/4':' no wi-fi';
  $('topst').textContent=s.board+' v'+s.version+'  '+(conn[s.conn]||'')+' '+bars+(s.battery<0?'':'  '+s.battery+'%');
  const rows=[['firmware','v'+s.version+' on '+s.board],['mode',setupMode?'setup (AP '+s.apSsid+')':'joined to Wi-Fi'],
    ['wi-fi',s.ssid?s.ssid+', '+s.rssiBars+'/4':'not connected'],['address',s.ip||'none'],
    ['battery',s.battery<0?'unknown':s.battery+'%'],['chat',conn[s.conn]||'unknown'],
    ['account',s.signedIn?(s.email||'signed in')+(s.verified?'':' (email not verified)'):'not signed in'],
    ['gps',s.gps||'none'],['time',s.time||'not set'],['zone',s.zone||'none']];
  const dl=$('statuslist');dl.innerHTML='';
  for(const[k,v]of rows){const dt=document.createElement('dt'),dd=document.createElement('dd');dt.textContent=k;dd.textContent=v;dl.append(dt,dd);}
  const ul=$('saved');ul.innerHTML='';
  if(!s.saved.length){const li=document.createElement('li');li.textContent='No saved networks yet.';li.className='none';ul.appendChild(li);}
  s.saved.forEach((n,i)=>{const li=document.createElement('li'),sp=document.createElement('span'),b=document.createElement('button');
    sp.textContent=(i+1)+'. '+n;b.type='button';b.textContent='Remove';
    b.onclick=async()=>{if(!confirm('Remove '+n+'?'))return;show('wifimsg',await call('DELETE','/api/wifi?ssid='+encodeURIComponent(n)));load();};
    li.append(sp,b);ul.appendChild(li);});
  $('acct').textContent=s.signedIn?'Signed in as '+(s.email||'?')+'.':'Not signed in.';
  if(!$('email').value&&s.email)$('email').value=s.email;
  $('pagestate').textContent=s.pagePasswordSet?'Set; type a new one to change it.':'Off.';
  $('apstate').textContent=s.apPasswordSet?'Set; type a new one to change it.':'Open.';
  $('pageoffrow').hidden=!s.pagePasswordSet;$('apopenrow').hidden=!s.apPasswordSet;
}

// ---- time zones ----
const listed=n=>zones.some(z=>z.iana===n);
// Browsers may report an older name for a listed zone.
const tzAlias={'Asia/Calcutta':'Asia/Kolkata','Asia/Saigon':'Asia/Ho_Chi_Minh','Asia/Katmandu':'Asia/Kathmandu',
  'Asia/Rangoon':'Asia/Yangon','Europe/Kiev':'Europe/Kyiv','America/Buenos_Aires':'America/Argentina/Buenos_Aires',
  'Pacific/Samoa':'Pacific/Pago_Pago','UTC':'Etc/UTC','Etc/GMT':'Etc/UTC','Etc/UCT':'Etc/UTC','GMT':'Etc/UTC'};
function browserZone(){try{const n=Intl.DateTimeFormat().resolvedOptions().timeZone;return tzAlias[n]||n;}catch(e){return '';}}
function fillZones(sel,custom){sel.innerHTML='';
  for(const z of zones){const o=document.createElement('option');o.value=z.iana;o.textContent=z.label+' ('+z.iana+')';sel.appendChild(o);}
  if(custom){const o=document.createElement('option');o.value='';o.textContent='Custom (POSIX TZ)';sel.appendChild(o);}}
async function loadZones(){if(zones.length)return;const r=await call('GET','/api/zones');zones=Array.isArray(r)?r:[];fillZones($('tzpick'),true);fillZones($('wtz'),true);}
function tzCustom(){const n=$('tzpick').value;$('tzrow').hidden=n!=='';const z=zones.find(z=>z.iana===n);if(z)$('tz').value=z.posix;}
$('tzpick').addEventListener('change',tzCustom);

// ---- prefs ----
function tsizeShow(){$('tsizeval').textContent=TSIZE_LABELS[+$('tsize').value];}
function ledLvlShow(){$('ledlvlval').textContent=cap(LEDLVLS[+$('ledlvl').value]);}
function brightShow(){$('brightval').textContent=$('bright').value+'%';}
function volShow(){const v=+$('vol').value;$('volval').textContent=v?v+'%':'Off';}
$('tsize').addEventListener('input',tsizeShow);$('ledlvl').addEventListener('input',ledLvlShow);
$('bright').addEventListener('input',brightShow);$('vol').addEventListener('input',volShow);
const showRow=(id,on)=>{$(id).hidden=!on;return on;};
async function loadPrefs(){const p=await call('GET','/api/prefs');if(p.tz===undefined)return;
  themes=p.themes||[];palette=p.palette||null;
  if(showRow('themerow',!!p.theme)){const sel=$('theme');sel.innerHTML='';
    for(const t of themes){const o=document.createElement('option');o.value=t.key;o.textContent=t.label;sel.appendChild(o);}sel.value=p.theme;}
  paintTheme();
  if(showRow('tsizerow',!!p.textSize)){$('tsize').max=p.noLarge?1:2;$('tsize').value=Math.max(0,TSIZES.indexOf(p.textSize));tsizeShow();}
  if(showRow('rotrow',p.rotation!==undefined))$('rot').value=String(p.rotation);
  if(showRow('brightrow',p.brightness!==undefined)){$('bright').value=p.brightness;brightShow();}
  $('scrsecs').value=String(p.screenOnSecs);$('scrsaver').value=p.screenSaver?'1':'0';
  if(showRow('chatrow',!!p.chatStyle))$('chat').value=p.chatStyle;
  if(showRow('unrrow',!!p.unreadFilter))$('unr').value=p.unreadFilter;
  if(showRow('spellrow',p.spellCheck!==undefined&&!p.phrases))$('spell').checked=p.spellCheck!==false;
  if(showRow('volrow',p.volume!==undefined)){$('vol').value=p.buzzer?p.volume:0;volShow();}
  if(showRow('buzzrow',p.buzzer!==undefined&&p.volume===undefined))$('buzzer').checked=!!p.buzzer;
  if(showRow('vibrow',p.tapVibrate!==undefined)){$('tapvib').checked=p.tapVibrate;$('alertvib').checked=p.alertVibrate;}
  if(showRow('ledrow',!!p.notifyLight)){showRow('ledtrow',true);$('ledcolor').value=p.notifyLight;for(const [id,k] of [['ledment','notifyMention'],['ledfeed','notifyFeed'],['ledcmail','notifyCmail']]){$(id+'l').hidden=!p[k];if(p[k])$(id).value=p[k];}$('ledtime').value=String(p.notifyTimeout);if(p.feedLightName)$('ledfeedn').textContent=p.feedLightName+' (Feed)';if(p.cmailLightName)$('ledcmailn').textContent=p.cmailLightName+' (C-Mail)';}else showRow('ledtrow',false);
  if(showRow('ledlvlrow',!!p.ledLevel)){$('ledlvl').value=LEDLVLS.indexOf(p.ledLevel);ledLvlShow();}
  if(showRow('pwrrow',!!p.powerLight))$('pwrlight').value=p.powerLight;
  if(showRow('wifirow',!!p.wifiJoined)){$('wifijoin').value=p.wifiJoined;$('wifiap').value=p.wifiSetup;$('wifilook').value=p.wifiLooking;}
  if(showRow('kbrow',!!p.kbLight))$('kblight').value=p.kbLight;
  $('lights').hidden=!$('lights').querySelector('.row:not([hidden])');
  $('webcfg').checked=p.webConfig!==false;
  if(showRow('phraseblock',!!p.phrases))$('phrases').value=p.phrases.join('\n');
  if(showRow('powerblock',p.idleSecs!==undefined)){$('idle').value=String(p.idleSecs);$('wake').value=String(p.wakeMins);}
  $('tz').value=p.tz;$('tzpick').value=listed(p.tzName)?p.tzName:'';tzCustom();$('ntp').value=p.ntpServer;
  // Setup mode keeps a zone already set; only the factory default takes the browser's.
  savedTzName=p.tzName;const keep=$('wtz').querySelector('option[value=""]');
  if(keep)keep.textContent='Keep current ('+p.tz+')';
  const bz=browserZone();$('wtz').value=p.tz==='UTC0'&&listed(bz)?bz:listed(p.tzName)?p.tzName:p.tz==='UTC0'?'Etc/UTC':'';}

// ---- actions: each its own button, sent at once ----
$('scanbtn').onclick=async()=>{const b=$('scanbtn');b.disabled=true;b.textContent='Scanning…';const r=await call('GET','/api/wifi/scan');
  const sel=$('ssidpick'),n=(r.networks||[]);sel.innerHTML='<option value="">'+n.length+' found: pick one</option>';
  for(const x of n){const o=document.createElement('option');o.value=o.textContent=x;sel.appendChild(o);}
  sel.hidden=false;b.disabled=false;b.textContent='Scan again';};
$('ssidpick').onchange=()=>{if($('ssidpick').value)$('ssid').value=$('ssidpick').value;};
$('addwifi').onclick=async()=>{
  // In setup mode the zone is saved first: saving the network restarts the device.
  if(setupMode&&$('wtz').value&&$('wtz').value!==savedTzName){const z=await call('POST','/api/prefs',{tzName:$('wtz').value});if(!z.ok){show('wifimsg',z);return;}}
  const r=await call('POST','/api/wifi',{ssid:$('ssid').value,password:$('wpass').value});show('wifimsg',r);if(r.ok)$('wpass').value='';setTimeout(load,800);};
$('signin').onclick=async()=>{$('acctmsg').textContent='Signing in…';$('acctmsg').className='msg';
  const r=await call('POST','/api/login',{email:$('email').value,password:$('cpass').value});$('cpass').value='';show('acctmsg',r);load();};
$('signout').onclick=async()=>{show('acctmsg',await call('POST','/api/logout',{}));setTimeout(load,800);};
$('resend').onclick=async()=>{show('acctmsg',await call('POST','/api/resend',{}));};
$('restart').onclick=async()=>{if(confirm('Restart the device?'))show('devmsg',await call('POST','/api/restart',{}));};
$('freset').onclick=async()=>{if(confirm('Erase Wi-Fi, the account and every setting, then restart into setup mode?')&&confirm('This can\'t be undone. Erase everything?'))
  show('devmsg',await call('POST','/api/factory-reset',{confirm:true}));};
$('restorefile').onchange=async()=>{
  const f=$('restorefile').files[0];if(!f)return;
  if(f.size>16384){show('backupmsg',{ok:false,message:'That file is over 16 KB, so it isn\'t a backup from this device.'});$('restorefile').value='';return;}
  if(!confirm('Restore '+f.name+' and restart?')){$('restorefile').value='';return;}
  let j;
  try{const r=await fetch('/api/restore',{method:'POST',headers:{'Content-Type':'application/yaml'},body:await f.text()});
    try{j=await r.json();}catch(e){j={ok:r.ok};}if(j.ok===undefined)j.ok=r.ok;}
  catch(e){j={ok:false,message:'No response from the device. Check it is on and on this network.'};}
  show('backupmsg',j);$('restorefile').value='';};
$('savesd').onclick=async()=>{show('backupmsg',await call('POST','/api/backup/sd',{}));loadBackup();};
async function loadBackup(){
  const s=await call('GET','/api/backup/sd');if(s.slot===undefined)return;
  $('sdblock').hidden=!s.slot;if(!s.slot)return;
  const ul=$('sdfiles');ul.innerHTML='';
  const none=t=>{const li=document.createElement('li');li.textContent=t;li.className='none';ul.appendChild(li);};
  if(!s.card)return none('No SD card in the device.');
  if(!(s.files||[]).length)none('No backups on the card yet.');
  for(const f of s.files||[]){
    const li=document.createElement('li'),sp=document.createElement('span');sp.textContent=f.name+', '+f.size+' B';li.appendChild(sp);
    const btn=(t,fn,cls)=>{const b=document.createElement('button');b.type='button';b.textContent=t;if(cls)b.className=cls;b.onclick=fn;li.appendChild(b);};
    btn('Restore',async()=>{if(!confirm('Restore '+f.name+' and restart?'))return;show('backupmsg',await call('POST','/api/backup/sd/restore?name='+encodeURIComponent(f.name)));});
    btn('Download',()=>{location.href='/api/backup/sd/download?name='+encodeURIComponent(f.name);});
    btn('Delete',async()=>{if(!confirm('Delete '+f.name+' from the card?'))return;show('backupmsg',await call('POST','/api/backup/sd/delete?name='+encodeURIComponent(f.name)));loadBackup();},'danger');
    ul.appendChild(li);}
}
)JS" RC_MASCOT_JS RC_OTA_JS R"JS(
load();loadBackup();loadSend();
reloadAll().then(buildNav);
setInterval(load,10000);
</script></body></html>)JS";

}  // namespace web
