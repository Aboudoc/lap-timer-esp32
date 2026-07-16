#pragma once
#include <Arduino.h>

// The embedded web app served in pit mode. Single self-contained page
// (no internet on the hotspot): racing-dashboard UI, live view with lap
// flash, session browser with charts, track manager, system tools.
// Installable on the phone's home screen (PWA manifest + icon).
//
// Color language (racing convention): purple = all-time best, green = gain /
// personal best, red = loss / hot, blue = cold.

const char MANIFEST_JSON[] PROGMEM = R"JSON({
"name":"LapTimer","short_name":"LapTimer","start_url":"/","display":"standalone",
"background_color":"#0a0a0c","theme_color":"#0a0a0c",
"icons":[{"src":"/apple-touch-icon.png","sizes":"180x180","type":"image/png"}]
})JSON";

const char WEBAPP_HTML[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
<link rel="manifest" href="/manifest.json">
<link rel="apple-touch-icon" href="/apple-touch-icon.png">
<title>LapTimer</title>
<style>
:root{--bg:#0a0a0c;--card:#151519;--line:#26262c;--txt:#f2f2f7;--mut:#8e8e93;
--grn:#00e676;--red:#ff453a;--pur:#bf5af2;--blu:#4da6ff;--amb:#ffd60a}
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
html{background:var(--bg)}
body{font-family:-apple-system,system-ui,sans-serif;background:var(--bg);
color:var(--txt);padding-bottom:86px;min-height:100vh}
/* header */
header{position:sticky;top:0;z-index:9;display:flex;align-items:center;gap:10px;
padding:14px 16px 10px;background:rgba(10,10,12,.86);backdrop-filter:blur(12px);
-webkit-backdrop-filter:blur(12px);border-bottom:1px solid var(--line)}
header::before{content:"";position:absolute;top:0;left:0;right:0;height:3px;
background:linear-gradient(90deg,var(--grn),transparent 60%)}
header h1{font-size:17px;font-weight:800;font-style:italic;letter-spacing:.5px}
header small{color:var(--mut);font-size:12px}
#dot{width:9px;height:9px;border-radius:50%;background:var(--red);margin-left:auto}
#dot.on{background:var(--grn);box-shadow:0 0 8px var(--grn)}
main{padding:12px 14px 0}
/* cards */
.card{background:var(--card);border:1px solid var(--line);border-radius:18px;
padding:16px;margin:0 0 12px}
h2{font-size:11px;color:var(--mut);text-transform:uppercase;letter-spacing:1.4px;
font-weight:600;margin:2px 2px 8px}
.num{font-variant-numeric:tabular-nums}
/* live */
.pills{display:flex;gap:8px;flex-wrap:wrap;margin-bottom:12px}
.pill{background:var(--card);border:1px solid var(--line);border-radius:99px;
padding:6px 13px;font-size:13px;color:var(--mut)}
.pill b{color:var(--txt)}
.hero{position:relative;text-align:center;padding:22px 16px 18px;overflow:hidden}
.hero .lbl{font-size:11px;letter-spacing:2px;color:var(--mut);text-transform:uppercase}
.hero .val{font-size:64px;font-weight:800;font-style:italic;letter-spacing:-2px;
line-height:1.1;padding-right:6px}
.stats{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}
.stat{background:var(--card);border:1px solid var(--line);border-radius:14px;
padding:10px 4px;text-align:center}
.stat b{display:block;font-size:16px;font-weight:700}
.stat span{font-size:10px;color:var(--mut);text-transform:uppercase;letter-spacing:.8px}
.stat.pb b{color:var(--pur)}
.tele{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-top:12px}
.t{background:var(--card);border:1px solid var(--line);border-radius:14px;
padding:10px;text-align:center}
.t b{display:block;font-size:20px;font-weight:700}
.t small{font-size:10px;color:var(--mut);text-transform:uppercase;letter-spacing:1px}
.t .u{font-size:11px;color:var(--mut);font-weight:400}
/* lap flash overlay */
#flash{position:fixed;inset:0;z-index:99;display:none;flex-direction:column;
justify-content:center;align-items:center;background:rgba(10,10,12,.94);
backdrop-filter:blur(6px)}
#flash.show{display:flex;animation:pop .25s ease-out}
@keyframes pop{from{transform:scale(.92);opacity:0}to{transform:scale(1);opacity:1}}
#flash .l1{font-size:14px;letter-spacing:3px;color:var(--mut)}
#flash .l2{font-size:70px;font-weight:800;font-style:italic;letter-spacing:-2px}
#flash .l3{font-size:26px;font-weight:700;margin-top:4px}
/* sessions */
.sess{display:flex;align-items:center;gap:12px;cursor:pointer}
.sess .bar{width:4px;align-self:stretch;border-radius:4px;background:var(--grn)}
.sess .inf{flex:1}
.sess .inf b{font-size:16px}
.sess .inf small{color:var(--mut);display:block;margin-top:2px}
.sess .bst{text-align:right}
.sess .bst b{font-size:18px;color:var(--grn)}
.sess .bst small{font-size:10px;color:var(--mut);display:block;text-transform:uppercase}
.bk{display:inline-flex;align-items:center;gap:6px;color:var(--mut);font-size:14px;
background:none;border:none;padding:4px 2px 10px}
table{width:100%;border-collapse:collapse;font-size:14px}
td,th{padding:7px 4px;text-align:right;border-bottom:1px solid var(--line)}
td:first-child,th:first-child{text-align:left}
th{color:var(--mut);font-weight:500;font-size:10px;text-transform:uppercase;
letter-spacing:1px}
tr.best td{color:var(--grn);font-weight:700}
td.up{color:var(--grn)}td.dn{color:var(--red)}
/* tracks */
.trk .top{display:flex;align-items:center;gap:8px;margin-bottom:10px}
.trk .top b{font-size:17px}
.act{font-size:10px;font-weight:700;letter-spacing:1px;color:#003b1d;
background:var(--grn);border-radius:99px;padding:3px 9px}
.trk .bst{color:var(--pur);font-weight:700}
input{background:#0f0f12;border:1px solid var(--line);color:var(--txt);
border-radius:11px;padding:10px;font-size:15px;width:140px}
button.b{background:#2a2a30;color:var(--txt);border:1px solid var(--line);
border-radius:11px;padding:10px 14px;font-size:14px;font-weight:600;margin:4px 6px 0 0}
button.go{background:var(--grn);color:#003b1d;border:none}
button.warn{background:none;color:var(--red);border:1px solid var(--red)}
/* system list */
.li{display:flex;align-items:center;gap:12px;padding:15px 4px;
border-bottom:1px solid var(--line);font-size:15px;cursor:pointer}
.li:last-child{border-bottom:none}
.li svg{width:20px;height:20px;color:var(--mut)}
.li span{margin-left:auto;color:var(--mut)}
.li.red{color:var(--red)}.li.red svg{color:var(--red)}
/* nav */
nav{position:fixed;bottom:0;left:0;right:0;z-index:9;display:flex;
background:rgba(16,16,20,.88);backdrop-filter:blur(14px);
-webkit-backdrop-filter:blur(14px);border-top:1px solid var(--line);
padding-bottom:env(safe-area-inset-bottom)}
nav button{flex:1;background:none;border:none;color:var(--mut);font-size:10px;
font-weight:600;letter-spacing:.5px;padding:9px 0 8px;position:relative}
nav button svg{display:block;width:23px;height:23px;margin:0 auto 3px}
nav button.on{color:var(--grn)}
nav button.on::before{content:"";position:absolute;top:0;left:30%;right:30%;
height:2px;border-radius:2px;background:var(--grn)}
.muted{color:var(--mut);font-size:14px;text-align:center;padding:26px 10px}
svg.ch{width:100%;height:auto;display:block;margin:8px 0 2px}
</style></head><body>
<header><h1>LAP TIMER</h1><small id="ver"></small><div id="dot"></div></header>
<main>
<section id="p-live">
 <div class="pills" id="lhead"></div>
 <div class="card hero"><div class="lbl" id="hlbl">CURRENT LAP</div>
  <div class="val num" id="hval">-:--.-</div></div>
 <div class="stats" id="lrow"></div>
 <div class="tele" id="ltele"></div>
</section>
<section id="p-sess" hidden>
 <div id="slist"></div><div id="sdetail"></div>
</section>
<section id="p-trk" hidden><div id="tlist"></div></section>
<section id="p-sys" hidden>
 <div class="card" style="padding:6px 16px">
  <div class="li" onclick="location='/laps.csv'">
   <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round"><path d="M12 3v12m0 0 5-5m-5 5-5-5M4 21h16"/></svg>
   Download lap log (CSV)<span>&rsaquo;</span></div>
  <div class="li" onclick="location='/update'">
   <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round"><path d="M12 21V9m0 0 5 5m-5-5-5 5M4 3h16"/></svg>
   Firmware update (OTA)<span>&rsaquo;</span></div>
  <div class="li" onclick="clearLog()">
   <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round"><path d="M4 7h16M9 7V4h6v3m-8 0 1 13h8l1-13"/></svg>
   Erase lap log<span>&rsaquo;</span></div>
  <div class="li red" onclick="exitPit()">
   <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2"
    stroke-linecap="round"><path d="M9 6a8 8 0 1 0 6 0M12 3v8"/></svg>
   Exit WiFi mode (reboot)<span>&rsaquo;</span></div>
 </div>
 <p class="muted">Firmware <b id="ver2"></b> &middot; hotspot 192.168.4.1</p>
</section>
</main>
<div id="flash"><div class="l1" id="fl1"></div><div class="l2 num" id="fl2"></div>
<div class="l3 num" id="fl3"></div></div>
<nav>
<button data-p="live" class="on"><svg viewBox="0 0 24 24" fill="none"
 stroke="currentColor" stroke-width="2" stroke-linecap="round">
 <path d="M5 19a9 9 0 1 1 14 0"/><path d="m12 13 4-6"/>
 <circle cx="12" cy="14" r="1.5" fill="currentColor"/></svg>Live</button>
<button data-p="sess"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
 stroke-width="2" stroke-linecap="round">
 <path d="M4 20v-6m6 6V4m6 16v-9M3 20h18"/></svg>Sessions</button>
<button data-p="trk"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
 stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
 <path d="M6 21V4m0 1c3-2.2 5 1.8 9 0l3-1v9c-3 2.2-5-1.8-9 0l-3 1"/></svg>Tracks</button>
<button data-p="sys"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor"
 stroke-width="2" stroke-linecap="round">
 <path d="M4 7h16M4 12h16M4 17h16"/><circle cx="9" cy="7" r="2" fill="var(--bg)"/>
 <circle cx="15" cy="12" r="2" fill="var(--bg)"/>
 <circle cx="8" cy="17" r="2" fill="var(--bg)"/></svg>System</button>
</nav>
<script>
const $=q=>document.querySelector(q);
function fmt(ms,c){if(ms==null||ms<=0)return"-:--";
const m=Math.floor(ms/60000),s=String(Math.floor(ms/1000)%60).padStart(2,"0");
return m+":"+s+"."+(c?String(Math.floor(ms/10)%100).padStart(2,"0"):Math.floor(ms/100)%10)}
function fdelta(d){const s=d<0?"-":"+";const a=Math.abs(d);
return s+Math.floor(a/1000)+"."+String(Math.floor(a/10)%100).padStart(2,"0")}
function tclr(v){return v<55?"var(--blu)":v<=90?"var(--grn)":"var(--red)"}
function pill(l,v){return `<div class="pill">${l} <b class="num">${v}</b></div>`}
function tele(l,v,u,c){return `<div class="t"><small>${l}</small>
<b class="num" ${c?`style="color:${c}"`:""}>${v}<span class="u"> ${u||""}</span></b></div>`}

/* ---- tabs ---- */
document.querySelectorAll("nav button").forEach(b=>b.onclick=()=>{
document.querySelectorAll("nav button").forEach(x=>x.classList.remove("on"));
b.classList.add("on");
["live","sess","trk","sys"].forEach(p=>$("#p-"+p).hidden=p!=b.dataset.p);
if(b.dataset.p=="sess")loadSessions();if(b.dataset.p=="trk")loadTracks();});

/* ---- live ---- */
let st=null,stAt=0,prevLaps=-1;
async function poll(){try{
const r=await fetch("/api/status",{cache:"no-store"});st=await r.json();stAt=Date.now();
$("#ver").textContent=st.version;$("#ver2").textContent=st.version;
$("#dot").classList.add("on");
if(prevLaps>=0&&st.laps>prevLaps)lapFlash();
prevLaps=st.laps;renderLive();
}catch(e){$("#dot").classList.remove("on")}}
function lapFlash(){$("#fl1").textContent="LAP "+st.laps;
$("#fl2").textContent=fmt(st.last,true);
const d=st.last&&st.allTime&&st.last<=st.allTime;
$("#fl2").style.color=d?"var(--pur)":"var(--txt)";
$("#fl1").style.color=d?"var(--pur)":"var(--mut)";
if(st.hasDelta||st.allTime){const dd=st.last-(st.allTime||st.best);
$("#fl3").textContent=d?"ALL-TIME BEST":fdelta(dd)+" vs best";
$("#fl3").style.color=d?"var(--pur)":(dd<0?"var(--grn)":"var(--red)");}
else $("#fl3").textContent="";
const f=$("#flash");f.classList.add("show");
setTimeout(()=>f.classList.remove("show"),4000);}
function renderLive(){if(!st)return;
$("#lhead").innerHTML=pill("SESSION",st.session)+pill("LAP",st.laps)+
`<div class="pill"><b>${st.track}</b></div>`+
pill("GPS",st.fix?st.sats+" sat":"&#9888;");
const cur=st.timing?st.current+(Date.now()-stAt):0;
const hv=$("#hval"),hl=$("#hlbl");
if(st.hasDelta){hl.textContent="DELTA vs BEST";hv.textContent=fdelta(st.delta);
hv.style.color=st.delta<0?"var(--grn)":"var(--red)";}
else{hl.textContent=st.timing?"CURRENT LAP":"WAITING FOR A LAP";
hv.textContent=st.timing?fmt(cur,false):"-:--.-";hv.style.color="var(--txt)";}
$("#lrow").innerHTML=
`<div class="stat"><b class="num">${st.timing?fmt(cur,false):"-"}</b><span>current</span></div>
<div class="stat"><b class="num">${fmt(st.last,true)}</b><span>last</span></div>
<div class="stat pb"><b class="num">${fmt(st.allTime||st.best,true)}</b><span>best</span></div>
<div class="stat"><b class="num">${fmt(st.tb,true)}</b><span>theor.</span></div>`;
let t=tele("Speed",st.speed,"km/h");
if(st.leanOk){const a=Math.abs(st.lean);
t+=tele("Lean",(st.lean<0?"&#9664; ":"")+a+(st.lean>0?" &#9654;":""),"&deg;");}
if(st.tfOk)t+=tele("Tire F",st.tf,"&deg;C",tclr(st.tf));
if(st.trOk)t+=tele("Tire R",st.tr,"&deg;C",tclr(st.tr));
if(st.ecu){t+=tele("RPM",st.rpm,"");t+=tele("Engine",st.coolant,"&deg;C",
st.coolant>102?"var(--red)":null);}
$("#ltele").innerHTML=t;}
setInterval(poll,1000);poll();
setInterval(()=>{if(st&&st.timing&&!st.hasDelta)renderLive()},100);

/* ---- sessions ---- */
let sessions=[];
async function loadSessions(){
const t=await(await fetch("/laps.csv",{cache:"no-store"})).text();
const lines=t.trim().split(/\r?\n/).slice(1);
const g={};
for(const l of lines){const r=l.split(",");if(r.length<6)continue;
const k=r[0]+"|"+r[2]+"|"+r[3];
(g[k]=g[k]||{date:r[0],track:r[2],sess:r[3],laps:[]}).laps.push(
{n:+r[4],t:Math.round(parseFloat(r[5])*1000),v:+r[6]||0,lean:+r[7]||0,
tf:+r[8]||0,tr:+r[9]||0});}
sessions=Object.values(g).reverse();
$("#sdetail").innerHTML="";
$("#slist").innerHTML=sessions.length?sessions.map((s,i)=>{
const best=Math.min(...s.laps.map(l=>l.t));
return `<div class="card sess" onclick="showSess(${i})"><div class="bar"></div>
<div class="inf"><b>${s.track} &middot; S${s.sess}</b>
<small>${s.date} &middot; ${s.laps.length} laps</small></div>
<div class="bst"><b class="num">${fmt(best,true)}</b><small>best</small></div></div>`
}).join(""):'<p class="muted">No laps yet.<br>Go ride &#127937;</p>';}
function chartSvg(vals){const w=340,h=150,pl=38,pr=12,pt=14,pb=22;
const mn=Math.min(...vals),mx=Math.max(...vals),sp=Math.max(200,mx-mn);
const sx=i=>pl+i*(w-pl-pr)/Math.max(1,vals.length-1);
const sy=v=>pt+(v-mn)*(h-pt-pb)/sp;
const line=vals.map((v,i)=>(i?"L":"M")+sx(i).toFixed(1)+" "+sy(v).toFixed(1)).join(" ");
const area=line+` L${sx(vals.length-1).toFixed(1)} ${h-pb} L${pl} ${h-pb} Z`;
const bi=vals.indexOf(mn);
return `<svg class="ch" viewBox="0 0 ${w} ${h}">
<defs><linearGradient id="g" x1="0" y1="0" x2="0" y2="1">
<stop offset="0" stop-color="#00e676" stop-opacity=".28"/>
<stop offset="1" stop-color="#00e676" stop-opacity="0"/></linearGradient></defs>
<line x1="${pl}" y1="${sy(mn)}" x2="${w-pr}" y2="${sy(mn)}" stroke="#bf5af2"
 stroke-dasharray="3 4" stroke-width="1"/>
<path d="${area}" fill="url(#g)"/>
<path d="${line}" fill="none" stroke="#00e676" stroke-width="2.5"
 stroke-linejoin="round" stroke-linecap="round"/>
<circle cx="${sx(bi)}" cy="${sy(mn)}" r="4.5" fill="#bf5af2"/>
<text x="${pl-6}" y="${sy(mn)+4}" fill="#bf5af2" font-size="10"
 text-anchor="end">${fmt(mn,false)}</text>
<text x="${pl-6}" y="${sy(mx)+4}" fill="#8e8e93" font-size="10"
 text-anchor="end">${fmt(mx,false)}</text>
<text x="${pl}" y="${h-8}" fill="#8e8e93" font-size="10">lap 1</text>
<text x="${w-pr}" y="${h-8}" fill="#8e8e93" font-size="10"
 text-anchor="end">lap ${vals.length}</text></svg>`}
function showSess(i){const s=sessions[i];
const ts=s.laps.map(l=>l.t);const best=Math.min(...ts);
const avg=ts.reduce((a,b)=>a+b,0)/ts.length;
const vmax=Math.max(...s.laps.map(l=>l.v)),lmax=Math.max(...s.laps.map(l=>l.lean));
$("#slist").innerHTML=`<button class="bk" onclick="loadSessions()">&lsaquo; Sessions</button>`;
$("#sdetail").innerHTML=`
<div class="pills">${pill("",s.track)}${pill("S",s.sess)}${pill("",s.date)}</div>
<div class="stats" style="margin-bottom:12px">
<div class="stat pb"><b class="num">${fmt(best,true)}</b><span>best</span></div>
<div class="stat"><b class="num">${fmt(avg,true)}</b><span>average</span></div>
<div class="stat"><b class="num">${vmax.toFixed(0)}</b><span>vmax</span></div>
<div class="stat"><b class="num">${lmax?lmax.toFixed(0)+"&deg;":"-"}</b><span>lean</span></div>
</div>
<div class="card"><h2>Lap times</h2>${s.laps.length>1?chartSvg(ts):'<p class="muted">One lap</p>'}</div>
<div class="card"><h2>Laps</h2><table>
<tr><th>Lap</th><th>Time</th><th>&Delta;</th><th>km/h</th><th>Lean</th><th>F&deg;</th><th>R&deg;</th></tr>
${s.laps.map(l=>{const d=l.t-best;
return `<tr class="${l.t==best?"best":""}"><td>${l.n}</td>
<td class="num">${fmt(l.t,true)}</td>
<td class="num ${d?"dn":"up"}">${d?"+"+ (d/1000).toFixed(2):"&#9733;"}</td>
<td class="num">${l.v.toFixed(0)}</td><td class="num">${l.lean?l.lean.toFixed(0):"-"}</td>
<td class="num" style="color:${l.tf?tclr(l.tf):"inherit"}">${l.tf?l.tf.toFixed(0):"-"}</td>
<td class="num" style="color:${l.tr?tclr(l.tr):"inherit"}">${l.tr?l.tr.toFixed(0):"-"}</td>
</tr>`}).join("")}
</table></div>`;
window.scrollTo({top:0,behavior:"smooth"});}

/* ---- tracks ---- */
async function post(u,o){await fetch(u,{method:"POST",
headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams(o)})}
async function loadTracks(){
const l=await(await fetch("/api/tracks",{cache:"no-store"})).json();
$("#tlist").innerHTML=l.length?l.map(t=>`<div class="card trk">
<div class="top"><b>${t.name}</b>${t.active?'<span class="act">ACTIVE</span>':""}
<span style="margin-left:auto" class="bst num">${fmt(t.best,true)}</span></div>
<input id="tn${t.id}" value="${t.name}" maxlength="15">
<button class="b go" onclick="tAct(${t.id},'select')">Select</button>
<button class="b" onclick="tAct(${t.id},'rename')">Rename</button>
<button class="b warn" onclick="if(confirm('Delete this track and its records?'))tAct(${t.id},'del')">Delete</button>
</div>`).join(""):'<p class="muted">No stored track yet.<br>Set the line while riding, it appears here.</p>';}
async function tAct(id,action){
await post("/track",{id,action,name:$("#tn"+id).value});loadTracks();}

/* ---- system ---- */
async function clearLog(){if(confirm("Erase the whole lap log?")){
await post("/clear",{});alert("Lap log erased")}}
async function exitPit(){if(confirm("Reboot into normal (track) mode?")){
await post("/exitpit",{});document.body.innerHTML=
'<p class="muted" style="padding-top:42vh;font-size:16px">Rebooting &mdash; see you on track &#127937;</p>'}}
</script></body></html>)HTML";
