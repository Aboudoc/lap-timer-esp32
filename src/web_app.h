#pragma once
#include <Arduino.h>

// The embedded web app served in pit mode. Single self-contained page
// (no internet on the hotspot): dark UI, live dashboard, session browser
// with charts, track manager, system tools. Installable on the phone's
// home screen (PWA manifest + icon).

const char MANIFEST_JSON[] PROGMEM = R"JSON({
"name":"LapTimer","short_name":"LapTimer","start_url":"/","display":"standalone",
"background_color":"#0e0e0e","theme_color":"#0e0e0e",
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
*{box-sizing:border-box;margin:0;padding:0;-webkit-tap-highlight-color:transparent}
body{font-family:-apple-system,system-ui,sans-serif;background:#0e0e0e;color:#eee;
padding-bottom:74px}
header{display:flex;justify-content:space-between;align-items:baseline;
padding:14px 16px 6px}
header h1{font-size:19px}header small{color:#888}
main{padding:0 12px}
.card{background:#1a1a1a;border-radius:14px;padding:14px;margin:10px 0}
.big{font-size:52px;font-weight:700;text-align:center;font-variant-numeric:tabular-nums;
letter-spacing:1px;padding:6px 0}
.pos{color:#ff5c5c}.neg{color:#37d67a}
.row{display:flex;justify-content:space-around;text-align:center}
.row div b{display:block;font-size:19px;font-variant-numeric:tabular-nums}
.row div span{font-size:11px;color:#888;text-transform:uppercase}
.chips{display:flex;flex-wrap:wrap;gap:8px;justify-content:center;margin-top:10px}
.chip{background:#262626;border-radius:9px;padding:6px 11px;font-size:14px}
.chip b{font-variant-numeric:tabular-nums}
nav{position:fixed;bottom:0;left:0;right:0;display:flex;background:#161616;
border-top:1px solid #2a2a2a;padding-bottom:env(safe-area-inset-bottom)}
nav button{flex:1;background:none;border:none;color:#777;font-size:13px;padding:13px 0 11px}
nav button.on{color:#37d67a;font-weight:600}
h2{font-size:13px;color:#888;text-transform:uppercase;margin:14px 4px 2px}
table{width:100%;border-collapse:collapse;font-size:14px;font-variant-numeric:tabular-nums}
td,th{padding:6px 4px;text-align:right;border-bottom:1px solid #262626}
td:first-child,th:first-child{text-align:left}
th{color:#888;font-weight:500;font-size:11px;text-transform:uppercase}
tr.best td{color:#37d67a;font-weight:600}
button.b{background:#28a;color:#fff;border:none;border-radius:9px;padding:9px 14px;
font-size:14px;margin:3px 4px 3px 0}
button.warn{background:#a33}
input{background:#262626;border:1px solid #333;color:#eee;border-radius:8px;
padding:8px;font-size:15px;width:130px}
.sess{display:flex;justify-content:space-between;align-items:center}
.sess small{color:#888}
svg{width:100%;height:auto;display:block;margin-top:6px}
.muted{color:#888;font-size:13px;text-align:center;padding:14px}
a{color:#4af}
</style></head><body>
<header><h1>&#127937; LapTimer</h1><small id="ver"></small></header>
<main>
<section id="p-live">
 <div class="card">
  <div class="chips" id="lhead"></div>
  <div class="big" id="lbig">-</div>
  <div class="row" id="lrow"></div>
  <div class="chips" id="lchips"></div>
  <div class="muted" id="lstat" hidden>connecting...</div>
 </div>
</section>
<section id="p-sess" hidden>
 <div id="slist"></div>
 <div id="sdetail"></div>
</section>
<section id="p-trk" hidden><div id="tlist"></div></section>
<section id="p-sys" hidden>
 <div class="card">
  <p style="margin-bottom:10px">Firmware <b id="ver2"></b></p>
  <button class="b" onclick="location='/laps.csv'">&#11015; Download lap log (CSV)</button>
  <button class="b warn" onclick="clearLog()">Erase lap log</button>
  <button class="b" onclick="location='/update'">Firmware update (OTA)</button>
  <button class="b warn" onclick="exitPit()">Exit WiFi mode (reboot)</button>
 </div>
</section>
</main>
<nav>
 <button data-p="live" class="on">Live</button>
 <button data-p="sess">Sessions</button>
 <button data-p="trk">Tracks</button>
 <button data-p="sys">System</button>
</nav>
<script>
const $=q=>document.querySelector(q);
function fmt(ms,c){if(ms==null||ms<=0)return"-:--";
const m=Math.floor(ms/60000),s=String(Math.floor(ms/1000)%60).padStart(2,"0");
return m+":"+s+"."+(c?String(Math.floor(ms/10)%100).padStart(2,"0"):Math.floor(ms/100)%10)}
function fdelta(d){const s=d<0?"-":"+";const a=Math.abs(d);
return s+Math.floor(a/1000)+"."+String(Math.floor(a/10)%100).padStart(2,"0")}
function chip(l,v){return `<div class="chip">${l} <b>${v}</b></div>`}

/* ---- tabs ---- */
document.querySelectorAll("nav button").forEach(b=>b.onclick=()=>{
document.querySelectorAll("nav button").forEach(x=>x.classList.remove("on"));
b.classList.add("on");
["live","sess","trk","sys"].forEach(p=>$("#p-"+p).hidden=p!=b.dataset.p);
if(b.dataset.p=="sess")loadSessions();if(b.dataset.p=="trk")loadTracks();});

/* ---- live ---- */
let st=null,stAt=0;
async function poll(){try{
const r=await fetch("/api/status");st=await r.json();stAt=Date.now();
$("#ver").textContent=st.version;$("#ver2").textContent=st.version;
$("#lstat").hidden=true;renderLive();
}catch(e){$("#lstat").hidden=false;$("#lstat").textContent="link lost, retrying..."}}
function renderLive(){if(!st)return;
$("#lhead").innerHTML=chip("S",st.session)+chip("Lap",st.laps)+chip("",st.track);
const cur=st.timing?st.current+(Date.now()-stAt):0;
const big=$("#lbig");
if(st.hasDelta){big.textContent=fdelta(st.delta);
big.className="big "+(st.delta<0?"neg":"pos");}
else{big.textContent=st.timing?fmt(cur,false):"-:--.-";big.className="big";}
$("#lrow").innerHTML=
`<div><b>${st.timing?fmt(cur,false):"-"}</b><span>current</span></div>
<div><b>${fmt(st.last,true)}</b><span>last</span></div>
<div><b>${fmt(st.best||st.allTime,true)}</b><span>best</span></div>
<div><b>${fmt(st.tb,true)}</b><span>theor.</span></div>`;
let c=chip("&#128752;",st.fix?st.sats:"no fix")+chip("km/h",st.speed);
if(st.leanOk)c+=chip("lean",st.lean+"&deg;");
if(st.tfOk)c+=chip("F",st.tf+"&deg;");if(st.trOk)c+=chip("R",st.tr+"&deg;");
if(st.ecu)c+=chip("rpm",st.rpm)+chip("&#127777;",st.coolant+"&deg;");
$("#lchips").innerHTML=c;}
setInterval(poll,1000);poll();
setInterval(()=>{if(st&&st.timing&&!st.hasDelta)renderLive()},100);

/* ---- sessions ---- */
let sessions=[];
async function loadSessions(){
const t=await(await fetch("/laps.csv")).text();
const lines=t.trim().split(/\r?\n/).slice(1);
const g={};
for(const l of lines){const r=l.split(",");if(r.length<6)continue;
const k=r[0]+"|"+r[2]+"|"+r[3];
(g[k]=g[k]||{date:r[0],track:r[2],sess:r[3],laps:[]}).laps.push(
{n:+r[4],t:Math.round(parseFloat(r[5])*1000),v:+r[6]||0,lean:+r[7]||0,tf:+r[8]||0,tr:+r[9]||0});}
sessions=Object.values(g).reverse();
$("#sdetail").innerHTML="";
$("#slist").innerHTML=sessions.length?sessions.map((s,i)=>{
const best=Math.min(...s.laps.map(l=>l.t));
return `<div class="card sess" onclick="showSess(${i})"><div><b>${s.track}</b> S${s.sess}
<small><br>${s.date} &middot; ${s.laps.length} laps</small></div>
<b class="neg">${fmt(best,true)}</b></div>`}).join("")
:'<p class="muted">No laps yet.</p>';}
function chartSvg(vals){const w=340,h=130,p=16;
const mn=Math.min(...vals),mx=Math.max(...vals),sp=Math.max(1,mx-mn);
const sx=i=>p+i*(w-2*p)/Math.max(1,vals.length-1);
const sy=v=>h-p-(v-mn)*(h-2*p)/sp;
const d=vals.map((v,i)=>(i?"L":"M")+sx(i).toFixed(1)+" "+sy(v).toFixed(1)).join(" ");
const bi=vals.indexOf(mn);
return `<svg viewBox="0 0 ${w} ${h}"><path d="${d}" fill="none" stroke="#37d67a"
stroke-width="2"/><circle cx="${sx(bi)}" cy="${sy(mn)}" r="4" fill="#37d67a"/></svg>`}
function showSess(i){const s=sessions[i];
const ts=s.laps.map(l=>l.t);const best=Math.min(...ts);
const avg=ts.reduce((a,b)=>a+b,0)/ts.length;
const vmax=Math.max(...s.laps.map(l=>l.v)),lmax=Math.max(...s.laps.map(l=>l.lean));
$("#sdetail").innerHTML=`<div class="card">
<div class="row"><div><b>${fmt(best,true)}</b><span>best</span></div>
<div><b>${fmt(avg,true)}</b><span>average</span></div>
<div><b>${vmax.toFixed(0)}</b><span>vmax</span></div>
<div><b>${lmax?lmax.toFixed(0)+"&deg;":"-"}</b><span>lean max</span></div></div>
${s.laps.length>1?chartSvg(ts):""}
<table><tr><th>Lap</th><th>Time</th><th>km/h</th><th>Lean</th><th>F&deg;</th><th>R&deg;</th></tr>
${s.laps.map(l=>`<tr class="${l.t==best?"best":""}"><td>${l.n}</td><td>${fmt(l.t,true)}</td>
<td>${l.v.toFixed(0)}</td><td>${l.lean?l.lean.toFixed(0):"-"}</td>
<td>${l.tf?l.tf.toFixed(0):"-"}</td><td>${l.tr?l.tr.toFixed(0):"-"}</td></tr>`).join("")}
</table></div>`;
$("#sdetail").scrollIntoView({behavior:"smooth"});}

/* ---- tracks ---- */
async function post(u,o){await fetch(u,{method:"POST",
headers:{"Content-Type":"application/x-www-form-urlencoded"},body:new URLSearchParams(o)})}
async function loadTracks(){
const l=await(await fetch("/api/tracks")).json();
$("#tlist").innerHTML=l.length?l.map(t=>`<div class="card">
<b>${t.active?"&#9654; ":""}${t.name}</b> &middot; best ${fmt(t.best,true)}<br>
<input id="tn${t.id}" value="${t.name}" maxlength="15">
<button class="b" onclick="tAct(${t.id},'select')">Select</button>
<button class="b" onclick="tAct(${t.id},'rename')">Rename</button>
<button class="b warn" onclick="if(confirm('Delete this track?'))tAct(${t.id},'del')">Del</button>
</div>`).join(""):'<p class="muted">No stored track yet.</p>';}
async function tAct(id,action){
await post("/track",{id,action,name:$("#tn"+id).value});loadTracks();}

/* ---- system ---- */
async function clearLog(){if(confirm("Erase the lap log?")){await post("/clear",{});alert("Done")}}
async function exitPit(){if(confirm("Reboot into normal (track) mode?")){
await post("/exitpit",{});document.body.innerHTML=
'<p class="muted" style="padding-top:40vh">Rebooting - reconnect at the track &#127937;</p>'}}
</script></body></html>)HTML";
