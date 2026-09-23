const $=s=>document.querySelector(s);
const view=$('#view');
const nav=$('#nav');
const pageTitle=$('#pageTitle');

const pages=[
  {name:'Dashboard',icon:'DB'},
  {name:'Device',icon:'DV'},
  {name:'Files',icon:'FL'},
  {name:'Notes',icon:'NT'},
  {name:'Tasks',icon:'TK'},
  {name:'Calendar',icon:'CL'},
  {name:'Bluetooth',icon:'BT'},
  {name:'Wi-Fi',icon:'WF'},
  {name:'Smart Home',icon:'HM'},
  {name:'Termo',icon:'TH'},
  {name:'Solar',icon:'SL'},
  {name:'MQTT',icon:'MQ'},
  {name:'GPIO',icon:'IO'},
  {name:'Serial',icon:'SR'},
  {name:'Automations',icon:'AU'},
  {name:'Notifications',icon:'NO'},
  {name:'Logs',icon:'LG'},
  {name:'Settings',icon:'ST'},
  {name:'System',icon:'SY'}
];

nav.innerHTML=pages.map(p=>'<button data-p="'+p.name+'"><span class="nav-icon">'+p.icon+'</span><span>'+p.name+'</span></button>').join('');
document.querySelectorAll('#nav button').forEach(b=>b.onclick=()=>openPage(b.dataset.p));
$('#logout').onclick=async()=>{await api('/api/logout',{method:'POST'});location.reload()};

function toast(message){
  const t=$('#toast');
  t.textContent=message;
  t.classList.add('show');
  clearTimeout(window.__toastTimer);
  window.__toastTimer=setTimeout(()=>t.classList.remove('show'),2200);
}

async function api(url,opt={}){
  const headers={...(opt.headers||{})};
  if(!(opt.body instanceof FormData)&&!headers['Content-Type'])headers['Content-Type']='application/json';
  const r=await fetch(url,{credentials:'same-origin',...opt,headers});
  if(r.status===401){
    showAuth(false);
    throw Error('unauthorized');
  }
  const text=await r.text();
  let data=text;
  try{data=JSON.parse(text)}catch{}
  if(!r.ok)throw Error((data&&data.error)||text||('HTTP '+r.status));
  return data;
}

function showAuth(setup){
  $('#auth').classList.remove('hidden');
  $('#setupBox').hidden=!setup;
  $('#loginBox').hidden=setup;
}

function hideAuth(){
  $('#auth').classList.add('hidden');
}

async function init(){
  try{
    const setup=await (await fetch('/api/setup/status')).json();
    if(!setup.setupComplete){
      showAuth(true);
      return;
    }
    const s=await api('/api/system/status');
    hideAuth();
    await openPage('Dashboard');
    updateShell(s);
    setInterval(refreshShell,15000);
  }catch(e){
    showAuth(false);
  }
}

async function login(){
  try{
    await api('/api/login',{method:'POST',body:JSON.stringify({password:$('#loginPassword').value})});
    hideAuth();
    toast('Signed in');
    await openPage('Dashboard');
    await refreshShell();
  }catch(e){
    $('#authMsg').textContent='Login failed. Check the administrator password.';
  }
}

async function finishSetup(){
  const body={
    deviceName:$('#deviceName').value.trim()||'PaperOS',
    language:$('#language').value,
    timezone:$('#timezone').value.trim()||'Europe/Rome',
    ssid:$('#ssid').value.trim(),
    wifiPassword:$('#wifiPassword').value,
    password:$('#adminPassword').value,
    sleepMinutes:15
  };
  try{
    const r=await api('/api/setup',{method:'POST',body:JSON.stringify(body)});
    $('#authMsg').textContent=r.ok?'Saved. PaperOS is rebooting…':'Setup failed';
  }catch(e){
    $('#authMsg').textContent=e.message||'Setup failed';
  }
}

function active(name){
  document.querySelectorAll('#nav button').forEach(b=>b.classList.toggle('active',b.dataset.p===name));
  pageTitle.textContent=name;
}

function updateShell(s,w){
  if(s){
    $('#sideDevice').textContent=s.deviceName||'M5Paper';
    $('#sideIp').textContent=s.ip||'Offline';
    $('#batteryChip').textContent='Battery '+s.batteryPercent+'%';
    $('#sideDot').classList.toggle('online',Boolean(s.ip&&s.ip!=='0.0.0.0'));
  }
  if(w){
    $('#networkChip').textContent=w.connected?'Wi-Fi '+w.ssid:'Setup AP';
    $('#networkChip').classList.toggle('good',Boolean(w.connected));
  }
}

async function refreshShell(){
  if(!$('#auth').classList.contains('hidden'))return;
  try{
    const [s,w]=await Promise.all([api('/api/system/status'),api('/api/wifi')]);
    updateShell(s,w);
  }catch{}
}

async function openPage(name){
  active(name);
  if(name==='Dashboard')return dashboard();
  if(name==='Device')return device();
  if(name==='Files')return files();
  if(name==='Notes')return notes();
  if(name==='Wi-Fi')return wifi();
  if(name==='Settings')return settings();
  if(name==='System')return systemPage();
  return coming(name);
}

function uptime(ms){
  const total=Math.floor(ms/1000);
  const d=Math.floor(total/86400);
  const h=Math.floor((total%86400)/3600);
  const m=Math.floor((total%3600)/60);
  return (d?d+'d ':'')+(h?h+'h ':'')+m+'m';
}

function bytes(n){
  n=Number(n||0);
  if(n>=1073741824)return(n/1073741824).toFixed(1)+' GB';
  if(n>=1048576)return(n/1048576).toFixed(1)+' MB';
  if(n>=1024)return(n/1024).toFixed(1)+' KB';
  return n+' B';
}

async function dashboard(){
  const [s,w]=await Promise.all([api('/api/system/status'),api('/api/wifi')]);
  updateShell(s,w);
  view.innerHTML=
    '<section class="hero">'+
      '<div><p class="eyebrow">LIVE DEVICE</p><h2>'+esc(s.deviceName)+'</h2>'+
      '<p>PaperOS is online and ready for local browser administration. Real device telemetry is shown below.</p></div>'+
      '<div class="hero-meta">'+
        '<div><span>Version</span><strong>'+esc(s.version)+'</strong></div>'+
        '<div><span>IP address</span><strong class="code">'+esc(s.ip)+'</strong></div>'+
        '<div><span>Uptime</span><strong>'+uptime(s.uptimeMs)+'</strong></div>'+
      '</div>'+
    '</section>'+
    '<div class="grid">'+
      metricCard('Battery',s.batteryPercent+'%',s.batteryMv+' mV','Power')+
      metricCard('Network',w.connected?'Online':'Setup AP',w.connected?esc(w.ssid):'PaperOS-Setup','Wi-Fi')+
      metricCard('Storage',s.sdMounted?bytes(s.sdFree):'No SD',s.sdMounted?'Free on microSD':'SD-backed apps unavailable','microSD')+
      metricCard('Heap',bytes(s.heapFree),bytes(s.heapMin)+' minimum','Memory')+
      metricCard('PSRAM',bytes(s.psramFree),bytes(s.psramTotal)+' total','Memory')+
      metricCard('Web console','paperos.local',esc(s.ip),'Local access')+
      '<section class="card full"><h3>Quick actions</h3><div class="toolbar">'+
        '<button id="qaHome">Show Home</button><button id="qaApps">Show Apps</button>'+
        '<button id="qaRefresh">Refresh e-paper</button><button id="qaDevice" class="primary">Remote device</button>'+
      '</div><div class="submetric">These controls call the live PaperOS REST API; they are not simulated UI elements.</div></section>'+
    '</div>';
  $('#qaHome').onclick=()=>act('/api/device/home','Home opened on M5Paper');
  $('#qaApps').onclick=()=>act('/api/device/apps','App launcher opened');
  $('#qaRefresh').onclick=()=>act('/api/device/refresh','Display refresh requested');
  $('#qaDevice').onclick=()=>openPage('Device');
}

function metricCard(title,value,detail,kicker){
  return '<section class="card"><h3>'+esc(kicker||title)+'</h3><div class="metric">'+esc(String(value))+'</div><div class="submetric">'+esc(String(detail||''))+'</div></section>';
}

async function device(){
  const s=await api('/api/system/status');
  view.innerHTML=
    '<div class="section-head"><div><h2>Remote device</h2><p>Control the physical M5Paper from this browser.</p></div></div>'+
    '<div class="grid">'+
      '<section class="card wide"><h3>Display controls</h3><div class="toolbar">'+
        '<button id="devHome">Home</button><button id="devApps">Apps</button><button id="devRefresh">Full refresh</button>'+
        '<button id="devSleep">Sleep</button><button id="devReboot" class="danger">Reboot</button>'+
      '</div><div class="submetric">Deep sleep turns off Wi-Fi, so remote wake is intentionally unavailable.</div></section>'+
      '<section class="card"><h3>Device</h3><div class="metric">'+esc(s.batteryPercent+'%')+'</div><div class="submetric">'+esc(s.deviceName)+' · '+esc(s.version)+'</div></section>'+
      '<section class="card full"><h3>Firmware OTA</h3>'+
        '<p class="submetric">Upload a compiled firmware .bin. PaperOS validates the update through the ESP32 Update API and reboots only after a successful write.</p>'+
        '<input id="fw" type="file" accept=".bin,application/octet-stream"><div class="toolbar" style="margin-top:10px"><button id="otaButton" class="primary">Upload firmware</button></div>'+
        '<p id="otaMsg" class="submetric"></p></section>'+
    '</div>';
  $('#devHome').onclick=()=>act('/api/device/home','Home opened');
  $('#devApps').onclick=()=>act('/api/device/apps','Apps opened');
  $('#devRefresh').onclick=()=>act('/api/device/refresh','Display refreshed');
  $('#devSleep').onclick=()=>{if(confirm('Put PaperOS into deep sleep?'))act('/api/device/sleep','Sleep requested')};
  $('#devReboot').onclick=()=>{if(confirm('Reboot PaperOS now?'))act('/api/device/reboot','Reboot requested')};
  $('#otaButton').onclick=ota;
}

async function act(url,message){
  try{
    await api(url,{method:'POST'});
    toast(message||'Command sent');
  }catch(e){
    toast('Command failed: '+e.message);
  }
}

async function ota(){
  const f=$('#fw').files[0];
  if(!f){toast('Choose a firmware .bin first');return}
  const fd=new FormData();
  fd.append('firmware',f);
  $('#otaMsg').textContent='Uploading '+f.name+'…';
  try{
    const r=await fetch('/api/ota',{method:'POST',body:fd,credentials:'same-origin'});
    const t=await r.text();
    $('#otaMsg').textContent=r.ok?'Update accepted. PaperOS is rebooting…':'OTA failed: '+t;
  }catch(e){
    $('#otaMsg').textContent='OTA connection failed';
  }
}

let cwd='/PaperOS';

async function files(){
  const d=await api('/api/files?path='+encodeURIComponent(cwd));
  const items=d.items||[];
  view.innerHTML=
    '<div class="section-head"><div><h2>File Manager</h2><p class="code">'+esc(cwd)+'</p></div></div>'+
    '<div class="toolbar"><button id="fileUp">Up</button><button id="fileNew">New folder</button><button id="fileUpload" class="primary">Upload</button><input id="upload" type="file" multiple hidden></div>'+
    '<div id="dropzone" class="dropzone">Drop files here to upload them to <strong>'+esc(cwd)+'</strong></div>'+
    '<section class="card full file-list" id="filelist"></section>';

  $('#fileUp').onclick=up;
  $('#fileNew').onclick=mkdir;
  $('#fileUpload').onclick=()=>$('#upload').click();
  $('#upload').onchange=e=>uploadFiles(e.target.files);

  const dz=$('#dropzone');
  ['dragenter','dragover'].forEach(n=>dz.addEventListener(n,e=>{e.preventDefault();dz.classList.add('drag')}));
  ['dragleave','drop'].forEach(n=>dz.addEventListener(n,e=>{e.preventDefault();dz.classList.remove('drag')}));
  dz.addEventListener('drop',e=>uploadFiles(e.dataTransfer.files));

  if(!items.length){
    $('#filelist').innerHTML='<div class="empty">This folder is empty.</div>';
    return;
  }

  $('#filelist').innerHTML=items.map(i=>{
    const path=join(cwd,i.name);
    const kind=i.directory?'DIR':'FILE';
    const meta=i.directory?'Folder':bytes(i.size);
    const open=i.directory?'<button class="open-file" data-path="'+escAttr(path)+'">Open</button>':'<button class="download-file" data-path="'+escAttr(path)+'">Download</button>';
    return '<div class="file-row"><div class="file-main"><span class="file-icon">'+kind+'</span><div><div class="file-name">'+esc(i.name)+'</div><div class="file-meta">'+esc(meta)+'</div></div></div>'+
      '<div class="file-actions">'+open+'<button class="rename-file" data-path="'+escAttr(path)+'" data-name="'+escAttr(i.name)+'">Rename</button><button class="delete-file danger" data-path="'+escAttr(path)+'">Delete</button></div></div>';
  }).join('');

  document.querySelectorAll('.open-file').forEach(b=>b.onclick=()=>{cwd=b.dataset.path;files()});
  document.querySelectorAll('.download-file').forEach(b=>b.onclick=()=>{location.href='/api/files/download?path='+encodeURIComponent(b.dataset.path)});
  document.querySelectorAll('.rename-file').forEach(b=>b.onclick=()=>renameItem(b.dataset.path,b.dataset.name));
  document.querySelectorAll('.delete-file').forEach(b=>b.onclick=()=>deleteItem(b.dataset.path));
}

function join(a,b){return(a+'/'+b).replaceAll('//','/')}

function up(){
  if(cwd==='/PaperOS')return;
  cwd=cwd.substring(0,cwd.lastIndexOf('/'))||'/PaperOS';
  if(!cwd.startsWith('/PaperOS'))cwd='/PaperOS';
  files();
}

async function mkdir(){
  const n=prompt('Folder name');
  if(!n)return;
  try{
    await api('/api/files/mkdir',{method:'POST',body:JSON.stringify({path:join(cwd,n)})});
    toast('Folder created');
    files();
  }catch(e){toast(e.message)}
}

async function uploadFiles(list){
  const filesArray=Array.from(list||[]);
  if(!filesArray.length)return;
  for(const f of filesArray){
    const fd=new FormData();
    fd.append('file',f);
    const r=await fetch('/api/files/upload?path='+encodeURIComponent(cwd),{method:'POST',body:fd,credentials:'same-origin'});
    if(!r.ok){toast('Upload failed: '+f.name);return}
  }
  toast(filesArray.length+' file(s) uploaded');
  files();
}

async function renameItem(path,currentName){
  const n=prompt('New name',currentName);
  if(!n||n===currentName)return;
  const base=path.substring(0,path.lastIndexOf('/'))||'/PaperOS';
  try{
    await api('/api/files/rename',{method:'POST',body:JSON.stringify({from:path,to:join(base,n)})});
    toast('Renamed');
    files();
  }catch(e){toast(e.message)}
}

async function deleteItem(path){
  if(!confirm('Delete '+path+'?'))return;
  try{
    await api('/api/files/delete',{method:'POST',body:JSON.stringify({path})});
    toast('Deleted');
    files();
  }catch(e){toast(e.message)}
}

async function notes(){
  const d=await api('/api/notes');
  const items=d.notes||[];
  view.innerHTML=
    '<div class="section-head"><div><h2>Notes</h2><p>Stored on the M5Paper microSD.</p></div><button id="newNote" class="primary">New note</button></div>'+
    '<div class="grid" id="notegrid">'+
      (items.length?items.map(n=>'<section class="card note-card" data-id="'+escAttr(n.id)+'"><h3>'+esc(n.category||'Note')+'</h3><div class="metric" style="font-size:1.15rem">'+esc(n.title||'Untitled')+'</div><div class="note-preview">'+(n.favorite?'Favorite · ':'')+'Tap to edit</div></section>').join(''):'<section class="card full empty">No notes yet.</section>')+
    '</div>';
  $('#newNote').onclick=()=>editNote('new');
  document.querySelectorAll('.note-card').forEach(c=>c.onclick=()=>editNote(c.dataset.id));
}

async function editNote(id){
  const n=id==='new'?{id:'note-'+Date.now(),title:'',body:'',category:'',favorite:false}:await api('/api/notes/item?id='+encodeURIComponent(id));
  view.innerHTML=
    '<div class="section-head"><div><h2>Note editor</h2><p>Autosaves after a short pause.</p></div><button id="backNotes">Back to notes</button></div>'+
    '<section class="card full">'+
      '<div class="form-grid"><label>Title<input id="nt" value="'+escAttr(n.title||'')+'" placeholder="Title"></label><label>Category<input id="nc" value="'+escAttr(n.category||'')+'" placeholder="Category"></label></div>'+
      '<label class="check-label"><input id="nf" type="checkbox" '+(n.favorite?'checked':'')+'> Favorite</label>'+
      '<label>Content<textarea id="nb" placeholder="Markdown / plain text">'+esc(n.body||'')+'</textarea></label>'+
      '<div class="toolbar"><button id="saveNote" class="primary">Save</button><button id="deleteNote" class="danger">Delete</button></div>'+
      '<div id="saveState" class="submetric"></div>'+
    '</section>';
  $('#backNotes').onclick=notes;
  $('#saveNote').onclick=()=>saveNote(n.id,false);
  $('#deleteNote').onclick=()=>deleteNote(n.id);
  let timer;
  ['nt','nc','nb','nf'].forEach(x=>$('#'+x).addEventListener('input',()=>{
    clearTimeout(timer);
    $('#saveState').textContent='Unsaved changes…';
    timer=setTimeout(()=>saveNote(n.id,true),800);
  }));
}

async function saveNote(id,quiet){
  const note={id,title:$('#nt').value,category:$('#nc').value,favorite:$('#nf').checked,body:$('#nb').value};
  try{
    await api('/api/notes/item?id='+encodeURIComponent(id),{method:'POST',body:JSON.stringify(note)});
    if(quiet){
      $('#saveState').textContent='Saved';
    }else{
      toast('Note saved');
      notes();
    }
  }catch(e){toast('Save failed: '+e.message)}
}

async function deleteNote(id){
  if(!confirm('Delete this note?'))return;
  try{
    await api('/api/notes/delete',{method:'POST',body:JSON.stringify({id})});
    toast('Note deleted');
    notes();
  }catch(e){toast(e.message)}
}

async function wifi(){
  const w=await api('/api/wifi');
  updateShell(null,w);
  view.innerHTML=
    '<div class="section-head"><div><h2>Wi-Fi</h2><p>Local connectivity and PaperOS browser access.</p></div></div>'+
    '<div class="grid">'+
      metricCard('Status',w.connected?'Connected':'Setup AP',w.connected?w.ssid:'PaperOS-Setup','Network')+
      metricCard('IP address',w.ip,w.connected?'Gateway '+w.gateway:'Captive portal','Address')+
      metricCard('Signal',w.connected?w.rssi+' dBm':'—',w.connected?'Live RSSI':'Waiting for Wi-Fi','Radio')+
      '<section class="card full"><h3>Connect & save network</h3><div class="form-grid"><label>SSID<input id="wssid" placeholder="Network name"></label><label>Password<input id="wpass" type="password" placeholder="Network password"></label></div><button id="connectWifi" class="primary">Connect</button></section>'+
    '</div>';
  $('#connectWifi').onclick=connectWifi;
}

async function connectWifi(){
  try{
    toast('Connecting…');
    await api('/api/wifi/connect',{method:'POST',body:JSON.stringify({ssid:$('#wssid').value,password:$('#wpass').value})});
    toast('Wi-Fi connected');
    setTimeout(wifi,500);
  }catch(e){toast('Connection failed')}
}

async function settings(){
  const s=await api('/api/settings');
  view.innerHTML=
    '<div class="section-head"><div><h2>Settings</h2><p>Persistent PaperOS device preferences.</p></div></div>'+
    '<section class="card full">'+
      '<div class="form-grid">'+
        '<label>Device name<input id="sname" value="'+escAttr(s.deviceName)+'"></label>'+
        '<label>Language<select id="slang"><option value="it">Italiano</option><option value="en">English</option></select></label>'+
        '<label>Timezone<input id="stz" value="'+escAttr(s.timezone)+'"></label>'+
        '<label>Auto sleep after (minutes)<input id="ssleep" type="number" min="0" value="'+Number(s.sleepMinutes||0)+'"></label>'+
        '<label>Scheduled wake (minutes, 0 = off)<input id="swake" type="number" min="0" value="'+Number(s.scheduledWakeMinutes||0)+'"></label>'+
        '<label style="display:flex;align-items:center;gap:8px;margin-top:28px"><input id="stouch" type="checkbox" '+(s.touchWakeEnabled?'checked':'')+'> Touch screen wake</label>'+
      '</div>'+
      '<p class="submetric">Auto sleep stays asleep until you wake the M5Paper. Timed wake is optional and separate.</p>'+
      '<button id="saveSettings" class="primary">Save settings</button>'+
    '</section>';
  $('#slang').value=s.language;
  $('#saveSettings').onclick=saveSettings;
}

async function saveSettings(){
  try{
    await api('/api/settings',{method:'POST',body:JSON.stringify({
      deviceName:$('#sname').value,
      language:$('#slang').value,
      timezone:$('#stz').value,
      sleepMinutes:Number($('#ssleep').value)||0,
      scheduledWakeMinutes:Number($('#swake').value)||0,
      touchWakeEnabled:$('#stouch').checked
    })});
    toast('Settings saved');
    refreshShell();
  }catch(e){toast('Save failed: '+e.message)}
}

async function systemPage(){
  const [s,w]=await Promise.all([api('/api/system/status'),api('/api/wifi')]);
  updateShell(s,w);
  view.innerHTML=
    '<div class="section-head"><div><h2>System monitor</h2><p>Live ESP32 and PaperOS diagnostics.</p></div><button id="sysRefresh">Refresh</button></div>'+
    '<div class="grid">'+
      metricCard('Heap',bytes(s.heapFree),bytes(s.heapMin)+' minimum','RAM')+
      metricCard('PSRAM',bytes(s.psramFree),bytes(s.psramTotal)+' total','PSRAM')+
      metricCard('Battery',s.batteryPercent+'%',s.batteryMv+' mV','Power')+
      metricCard('Uptime',uptime(s.uptimeMs),'Since last boot','System')+
      '<section class="card half"><h3>Network</h3><div class="card-line"><span>SSID</span><strong>'+esc(w.ssid)+'</strong></div><div class="card-line"><span>IP</span><strong class="code">'+esc(w.ip)+'</strong></div><div class="card-line"><span>MAC</span><strong class="code">'+esc(s.mac)+'</strong></div><div class="card-line"><span>RSSI</span><strong>'+esc(String(w.rssi))+' dBm</strong></div></section>'+
      '<section class="card half"><h3>Storage</h3><div class="card-line"><span>microSD</span><strong>'+(s.sdMounted?'Mounted':'Unavailable')+'</strong></div><div class="card-line"><span>Free</span><strong>'+bytes(s.sdFree)+'</strong></div><div class="card-line"><span>Firmware</span><strong>'+esc(s.version)+'</strong></div><div class="card-line"><span>Device</span><strong>'+esc(s.deviceName)+'</strong></div></section>'+
    '</div>';
  $('#sysRefresh').onclick=systemPage;
}

function coming(name){
  view.innerHTML=
    '<section class="hero"><div><p class="eyebrow">ROADMAP MODULE</p><h2>'+esc(name)+'</h2><p>This module is deliberately marked Coming Soon until its real backend is implemented. PaperOS does not simulate hardware features.</p></div><div class="hero-meta"><div><span>Status</span><strong>Coming Soon</strong></div><div><span>Release</span><strong>Future alpha</strong></div></div></section>'+
    '<div class="grid"><section class="card full soon"><h3>Design rule</h3><div class="metric" style="font-size:1.35rem">Real function first.</div><div class="submetric">When this module becomes available, every visible control will be connected to an implemented service or driver.</div></section></div>';
}

function esc(s=''){
  return String(s).replace(/[&<>]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;'}[c]));
}

function escAttr(s=''){
  return esc(s).replaceAll('"','&quot;').replaceAll("'",'&#39;');
}

$('#loginPassword').addEventListener('keydown',e=>{if(e.key==='Enter')login()});
init();
