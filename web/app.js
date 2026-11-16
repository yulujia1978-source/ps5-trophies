import {grades,normalize,summary,filterTrophies} from './model.js';
const app=document.querySelector('#app'),connection=document.querySelector('#connection');
const runtimeVersion=document.querySelector('#runtime-version');
const pageVersion=document.body.dataset.pageVersion;
const themeSelect=document.querySelector('#theme-select');
const themeColors={blue:'#03142f',light:'#f7f8fa',graphite:'#17191e'};
function applyTheme(value){
  const theme=Object.hasOwn(themeColors,value)?value:'blue';
  document.body.dataset.theme=theme;
  document.documentElement.style.colorScheme=theme==='light'?'light':'dark';
  document.documentElement.style.backgroundColor=themeColors[theme];
  document.querySelector('meta[name="theme-color"]').content=themeColors[theme];
  themeSelect.value=theme;
}
let savedTheme='blue';try{savedTheme=localStorage.getItem('ps5-trophies.theme')??'blue';}catch{}
applyTheme(savedTheme);
themeSelect.addEventListener('change',()=>{applyTheme(themeSelect.value);try{localStorage.setItem('ps5-trophies.theme',themeSelect.value);}catch{}});
window.addEventListener('storage',event=>{if(event.key==='ps5-trophies.theme')applyTheme(event.newValue);});
function showVersion(version){
  runtimeVersion.className=version&&version!==pageVersion?'mismatch':version?'':'unavailable';
  runtimeVersion.textContent=demo?`界面演示 · 页面 v${pageVersion}`:version?`运行版本 v${version}${version!==pageVersion?` · 页面 v${pageVersion}，请启动新版插件`:''}`:`未连接 · 页面 v${pageVersion}`;
}
const demo=new URLSearchParams(location.search).get('demo')==='1';
const state={source:'',users:[],uid:'',games:[],warnings:[],updated:null,loading:false,error:'',query:'',order:'recent',status:'all',grade:'all',sort:'id',trophyQuery:'',generation:0};
const labels={platinum:'白金',gold:'金',silver:'银',bronze:'铜',unknown:'未知'};
const esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const cup=(cls,grade)=>`<svg class="${cls??''}" aria-hidden="true"><use href="#${grade==='platinum'?'platinum':'cup'}"/></svg>`;
const arrow=()=>'<svg class="chevron" aria-hidden="true"><use href="#arrow"/></svg>';
const art=(url,cls='cover')=>`<div class="art ${cls}">${cup('fallback-cup')}${url?`<img src="${esc(url)}" alt="" loading="lazy">`:''}</div>`;
const symbol=(id,cls='')=>`<svg class="${cls}" aria-hidden="true"><use href="#${id}"/></svg>`;
const pct=n=>n===null?'—':`${n}%`;
const counts=s=>`<div class="counts">${grades.map(g=>`<span class="count" title="${labels[g]}奖杯"><span class="${g}">${cup('',g)}</span>${s.counts[g]}</span>`).join('')}</div>`;
const progress=p=>`<div class="progress-track" role="img" aria-label="完成进度 ${pct(p)}"><span style="width:${p??0}%"></span></div>`;
const latest=g=>Math.max(0,...g.trophies.map(t=>t.date?Date.parse(t.date):0));
function user(){return state.users.find(u=>u.id===state.uid);}
function summaryPanel(g){return `<div class="profile"><span class="avatar">${esc(Array.from(user()?.name??'本')[0])}</span>${esc(user()?.name)}</div><section class="summary"><div class="summary-top"><div class="stat"><strong>${g.earned}</strong><span>已获得</span></div><div class="ring"><svg viewBox="0 0 90 90"><circle cx="45" cy="45" r="40"/><circle class="fill" cx="45" cy="45" r="40" style="stroke-dashoffset:${251.33*(1-(g.progress??0)/100)}"/></svg><b>${pct(g.progress)}<small>完成率</small></b></div><div class="stat"><strong>${g.total}</strong><span>总计奖杯</span></div></div><div class="big-counts">${grades.map(k=>`<div class="big-count"><span class="${k}">${cup('',k)}</span>${g.counts[k]}</div>`).join('')}</div></section><p class="summary-note">${g.unknown?`${g.unknown} 项状态未知`:''}</p>`;}
function shell(content){const route=location.hash.slice(1).split('/').filter(Boolean);const tab=['stats','profile','search'].includes(route[0])?route[0]:'games';document.body.dataset.view=route.length&&tab==='games'?'detail':'library';document.querySelectorAll('[data-tab]').forEach(a=>{a.classList.toggle('active',a.dataset.tab===tab);if(a.dataset.tab===tab)a.setAttribute('aria-current','page');else a.removeAttribute('aria-current');});app.innerHTML=content;app.querySelectorAll('img').forEach(img=>img.addEventListener('error',()=>img.remove(),{once:true}));app.querySelectorAll('[data-refresh]').forEach(b=>b.onclick=refresh);}
function render(){
  const route=location.hash.slice(1).split('/').filter(Boolean);const game=state.games.find(g=>g.id===route[0]);
  if(state.loading&&!state.games.length){shell('<div class="loading"><span class="spinner"></span><p>正在读取主机奖杯记录…</p></div>');return;}
  if(state.error){shell(`<div class="empty"><h2>暂时无法读取奖杯</h2><p>${esc(state.error)}</p><p>请确认 PS5 奖杯插件已启动，然后重新读取。</p><button class="refresh" data-refresh>重新读取</button></div>`);return;}
  if(['stats','profile'].includes(route[0])){renderAccount(route[0]);return;}
  if(route[0]==='search'){renderLibrary(true);return;}
  if(route.length&&game){renderDetail(game,route[1]);return;}
  if(route.length&&!game){shell('<div class="empty"><h2>当前用户没有此游戏记录</h2><a class="refresh" href="#/">返回游戏列表</a></div>');return;}
  renderLibrary();
}
function renderLibrary(searchPage=false){
  const filtersOpen=searchPage||(document.querySelector('.library-filter')?.open??false);
  let games=state.games.filter(g=>g.name.toLowerCase().includes(state.query.toLowerCase())||g.id.toLowerCase().includes(state.query.toLowerCase()));
  games.sort((a,b)=>state.order==='progress'?(b.progress??-1)-(a.progress??-1):state.order==='name'?a.name.localeCompare(b.name,'zh-CN'):latest(b)-latest(a)||a.name.localeCompare(b.name,'zh-CN'));
  shell(`<div class="heading"><h1>${searchPage?'搜索游戏':'我的游戏'}</h1><div class="controls"><select id="user" class="control user-select" aria-label="选择本地用户">${state.users.map(u=>`<option value="${esc(u.id)}" ${u.id===state.uid?'selected':''}>${esc(u.name)}</option>`).join('')}</select><button class="refresh" data-refresh ${state.loading?'disabled':''} aria-label="刷新奖杯">${state.loading?'读取中…':'刷新'}</button></div></div>
  ${demo?'<div class="notice warning">界面演示 · 下方是用于预览的虚构记录，并非 PS5 实机读取结果。</div>':''}
  ${state.source==='fixture'?'<div class="notice warning">本地联调 · 使用构造的奖杯文件验证读取链路，非 PS5 实机数据。</div>':''}
  ${user()?.error?`<div class="notice warning">当前用户读取失败：${esc(user().error)}</div>`:''}
  <details class="library-filter"><summary><svg><use href="#sort"/></svg><span>搜索与排序</span></summary><div class="filter-content"><input class="search" id="search" placeholder="搜索游戏名称或奖杯编号" aria-label="搜索游戏" value="${esc(state.query)}"><select id="order" class="control" aria-label="游戏排序"><option value="recent">最近获得</option><option value="progress">完成进度</option><option value="name">游戏名称</option></select></div></details>
  <section class="games">${games.map(g=>`<article class="game-card"><a class="game-main" href="#/${esc(g.id)}" aria-label="查看 ${esc(g.name)} 的奖杯">${g.icon?`<img class="card-backdrop" src="${esc(g.icon)}" alt="" loading="lazy">`:''}<div class="game-top">${art(g.icon)}<div class="game-copy"><h2>${esc(g.name)}</h2><span class="platform">${g.platform}</span><div class="game-facts"><div><span>已获得奖杯</span><strong><span class="platinum">${cup('','platinum')}</span>${pct(g.progress)}</strong>${progress(g.progress)}</div><div class="earned-fact"><span>奖杯数量</span><strong>${g.earned}<small> / ${g.total}</small></strong></div></div></div></div><div class="game-bottom"><div class="game-bottom-left">${symbol('plain-cup','cup')}<h3>奖杯 <small>（共 ${g.total} 个）</small></h3></div>${counts(g)}${arrow()}</div></a><a class="game-assist" href="#/${esc(g.id)}/all">${symbol('gamepad')}<span><strong>奖杯详情</strong><small>解锁条件 / 分类筛选 / 获得时间</small></span>${arrow()}</a></article>`).join('')}</section>
  ${!games.length?`<div class="empty"><h2>${state.query?'没有找到相关游戏':'还没有可展示的奖杯游戏'}</h2><p>${state.query?'换个关键词试试。':'仅展示已确认获得至少一座奖杯的 PS5 游戏。'}</p>${!state.users.length?'<p>主机未发现可读取的本地用户。</p>':''}</div>`:''}
  ${state.warnings.length?`<details><summary>${state.warnings.length} 项读取提示</summary>${state.warnings.map(w=>`<div class="error-line">${esc(w)}</div>`).join('')}</details>`:''}
  <p class="last-update">${state.updated?`最近读取 ${state.updated.toLocaleTimeString('zh-CN')}`:''} · 当前支持 PS5 原生奖杯记录</p>`);
  document.querySelector('.library-filter').open=filtersOpen;
  document.querySelector('#user').onchange=e=>{state.uid=e.target.value;state.query='';location.hash='/';loadUser();};
  const search=document.querySelector('#search');search.oninput=e=>{const pos=e.target.selectionStart;state.query=e.target.value;renderLibrary(searchPage);const el=document.querySelector('#search');el.focus();el.setSelectionRange(pos,pos);};
  const order=document.querySelector('#order');order.value=state.order;order.onchange=e=>{state.order=e.target.value;renderLibrary(searchPage);};
}
function renderDetail(game,gid){
  const group=gid==='all'?{id:'all',name:'所有奖杯',trophies:game.trophies,...summary(game.trophies)}:gid?game.groups.find(g=>g.id===gid):null;
  if(gid&&!group){shell('<div class="empty"><h2>没有找到该奖杯分组</h2><a href="#/">返回游戏列表</a></div>');return;}
  shell(`<nav class="page-nav"><a class="back" href="${group?`#/${game.id}`:'#/'}">${arrow()}${group?'奖杯总览':'全部游戏'}</a><span class="breadcrumb">/ ${esc(group?.name??game.name)}</span><a class="detail-home" href="#/">${symbol('home')}返回首页</a></nav>
  ${demo?'<div class="notice warning">界面演示 · 虚构奖杯记录</div>':state.source==='fixture'?'<div class="notice warning">本地联调 · 构造的奖杯文件</div>':''}
  <div class="detail-layout"><aside><div class="game-hero">${game.icon?`<img class="hero-backdrop" src="${esc(game.icon)}" alt="">`:''}<div class="game-identity">${art(game.icon,'')}<div><h1>${esc(game.name)}</h1><span class="platform">${game.platform}</span></div></div></div>${summaryPanel(group??game)}${game.unknown?'<div class="notice warning">部分奖杯状态无法确认，完成百分比暂不显示。</div>':''}</aside><section id="detail-content"></section></div>`);
  const panel=document.querySelector('#detail-content');
  if(!group){panel.innerHTML=`${game.groups.map((g,i)=>`${i===0?'<h2 class="section-title">正式版游戏</h2>':i===1?'<h2 class="section-title">追加内容</h2>':''}<a class="group" href="#/${game.id}/${encodeURIComponent(g.id)}">${art(game.icon,'')}<div class="group-copy"><div class="group-name">${esc(i===0?game.name:g.name)}</div><div class="group-line">${counts(g)}<b>${pct(g.progress)}</b></div>${progress(g.progress)}</div></a>`).join('')}<a class="all-trophies" href="#/${game.id}/all">查看所有奖杯 <span>${game.total}</span>${arrow()}</a>`;
  }else renderTrophies(group,panel);
  panel.querySelectorAll('img').forEach(img=>img.addEventListener('error',()=>img.remove(),{once:true}));
}
function trophyCard(t){return `<article class="trophy-card ${t.unlocked===true?'earned':t.unlocked===false?'locked':'unknown'}"><div class="art trophy-art ${t.grade}">${cup('',t.grade)}${t.icon?`<img src="${esc(t.icon)}" alt="" loading="lazy">`:''}${t.unlocked===false?'<svg class="lock-mark"><use href="#lock"/></svg>':''}</div><div class="trophy-copy"><h3>${esc(t.name)} ${t.hidden&&t.unlocked!==true?'<span class="badge">隐藏</span>':''}</h3><p>${esc(t.detail||'暂无奖杯说明')}</p><div class="trophy-meta"><span class="count ${t.grade}" title="${labels[t.grade]}奖杯">${cup('',t.grade)}</span><span>${t.unlocked===true?(t.date?new Date(t.date).toLocaleString('zh-CN',{year:'numeric',month:'2-digit',day:'2-digit',hour:'2-digit',minute:'2-digit'}):'已获得 · 时间未知'):t.unlocked===false?'未获得':'状态未知'}</span>${t.unlocked===true?'<svg class="earned-mark"><use href="#plain-cup"/></svg>':''}</div></div></article>`;}
function renderTrophies(group,panel){
  const filtersOpen=panel.querySelector('.trophy-filter')?.open??false;
  const list=filterTrophies(group.trophies,{status:state.status,grade:state.grade,sort:state.sort,query:state.trophyQuery});
  const recent=group.trophies.filter(t=>t.unlocked&&t.date).sort((a,b)=>b.date.localeCompare(a.date))[0];
  panel.innerHTML=`<details class="trophy-filter"><summary aria-label="奖杯筛选与排序"><svg><use href="#sort"/></svg></summary><div class="filter-content"><input id="trophy-search" class="search list-search" placeholder="搜索奖杯名称或说明" aria-label="搜索奖杯" value="${esc(state.trophyQuery)}"><div class="list-tools"><select class="control" id="status" aria-label="按解锁状态筛选"><option value="all">全部状态</option><option value="earned">已获得</option><option value="locked">未获得</option><option value="unknown">状态未知</option></select><select class="control" id="grade" aria-label="按奖杯等级筛选"><option value="all">全部等级</option>${grades.map(g=>`<option value="${g}">${labels[g]}奖杯</option>`).join('')}</select><select class="control" id="sort" aria-label="奖杯排序"><option value="id">默认顺序</option><option value="recent">最近获得</option><option value="grade">奖杯等级</option></select></div></div></details>${recent&&state.status==='all'&&!state.trophyQuery&&state.grade==='all'?`<h2 class="section-title">最近获得</h2>${trophyCard(recent)}`:''}<h2 class="section-title">${state.status==='all'?'所有奖杯':state.status==='earned'?'已获得的奖杯':state.status==='locked'?'尚未获得的奖杯':'状态未知的奖杯'} <span class="muted">· ${list.length}</span></h2>${list.map(trophyCard).join('')}${!list.length?'<div class="empty">没有符合条件的奖杯</div>':''}`;
  panel.querySelector('.trophy-filter').open=filtersOpen;
  for(const key of ['status','grade','sort']){const el=panel.querySelector(`#${key}`);el.value=state[key];el.onchange=e=>{state[key]=e.target.value;renderTrophies(group,panel);};}
  panel.querySelector('#trophy-search').oninput=e=>{const pos=e.target.selectionStart;state.trophyQuery=e.target.value;renderTrophies(group,panel);const el=panel.querySelector('#trophy-search');el.focus();el.setSelectionRange(pos,pos);};
  panel.querySelectorAll('img').forEach(img=>img.addEventListener('error',()=>img.remove(),{once:true}));
}
function accountControls(){return `<div class="heading"><h1>${location.hash.includes('stats')?'奖杯统计':'个人资料'}</h1><div class="controls"><select id="account-user" class="control" aria-label="选择本地用户">${state.users.map(u=>`<option value="${esc(u.id)}" ${u.id===state.uid?'selected':''}>${esc(u.name)}</option>`).join('')}</select><button class="refresh" data-refresh>刷新</button></div></div>`;}
function renderAccount(tab){
  const trophies=state.games.flatMap(g=>g.trophies),total=summary(trophies);
  shell(`${accountControls()}${demo?'<div class="notice warning">界面演示 · 虚构记录</div>':''}${state.source==='fixture'?'<div class="notice warning">本地联调 · 构造数据</div>':''}${tab==='stats'?`${summaryPanel(total)}<div class="account-grid"><div class="info-tile"><strong>${state.games.length}</strong><span>获得过奖杯的游戏</span></div><div class="info-tile"><strong>${state.games.filter(g=>g.progress===100).length}</strong><span>全部完成的游戏</span></div></div><h2 class="section-title">游戏完成情况</h2>${state.games.map(g=>`<a class="group" href="#/${g.id}">${art(g.icon,'')}<div class="group-copy"><div class="group-name">${esc(g.name)}</div><div class="group-line">${counts(g)}<b>${pct(g.progress)}</b></div>${progress(g.progress)}</div>${arrow()}</a>`).join('')}`:`<section class="profile-card"><div class="profile-avatar">${symbol('person')}</div><h2>${esc(user()?.name??'暂无本地用户')}</h2><p>PS5 本地用户</p><div class="profile-data"><span>用户 ID</span><strong>${esc(state.uid||'—')}</strong></div><div class="profile-data"><span>奖杯游戏</span><strong>${state.games.length}</strong></div><div class="profile-data"><span>已获得奖杯</span><strong>${total.earned}</strong></div><p class="profile-note">本地记录 · 无需登录 PSN</p></section>`}`);
  document.querySelector('#account-user').onchange=e=>{state.uid=e.target.value;loadUser();};
}
async function json(url){const response=await fetch(url,{cache:'no-store',signal:AbortSignal.timeout(20000)});if(!response.ok){let msg=`HTTP ${response.status}`;try{msg=(await response.json()).error??msg;}catch{}throw Error(msg);}return response.json();}
async function loadUser(){
  const generation=++state.generation;state.games=[];state.warnings=[];state.loading=true;render();
  const sets=user()?.sets??[];const uid=state.uid;let cursor=0;const games=[],warnings=[];
  async function worker(){while(cursor<sets.length){const s=sets[cursor++];if(generation!==state.generation)return;
    if(!s.readable||s.unknown)warnings.push(`${s.id}：${s.readable?'部分解锁状态未知':'无法识别或读取解锁记录'}`);
    if(!s.earned)continue;
    const base=`/api/set/${uid}/${s.id}`;
    try{const [conf,meta,rows]=await Promise.all([json(`${base}/conf`),json(`${base}/meta`).catch(()=>null),json(`${base}/state`)]);const game=normalize(s.id,conf,meta,rows,base);if(game.earned>0)games.push(game);if(!meta)warnings.push(`${s.id}：名称或语言元数据不可用`);}catch(e){warnings.push(`${s.id}：${e.message}`);}
  }}
  await Promise.all([worker(),worker(),worker()]);if(generation!==state.generation)return;state.games=games;state.warnings=warnings;state.loading=false;state.updated=new Date();render();
}
async function refresh(){
  if(state.loading)return;state.loading=true;state.error='';render();
  if(demo){showVersion(null);const data=makeDemo();state.users=data.users;state.uid='demo';state.games=data.games;state.loading=false;state.updated=new Date();connection.innerHTML='<span></span>界面演示';connection.classList.add('off');render();return;}
  try{const data=await json('/api/catalog');showVersion(data.version??null);state.source=data.source;state.users=data.users;if(!state.users.some(u=>u.id===state.uid))state.uid=state.users[0]?.id??'';connection.innerHTML=data.source==='fixture'?'<span></span>本地测试数据':'<span></span>已连接主机';connection.classList.remove('off');await loadUser();}catch(e){showVersion(null);state.error=e.message;state.loading=false;connection.innerHTML='<span></span>连接失败';connection.classList.add('off');render();}
}
let liveSession=null,liveDisconnected=false;
async function watchConnection(){
  if(demo)return;
  try{
    const response=await fetch('/api/status',{cache:'no-store',signal:AbortSignal.timeout(3000)});
    if(!response.ok)throw Error('offline');const status=await response.json();
    if(liveDisconnected||status.version!==pageVersion||(liveSession&&liveSession!==status.session)){location.reload();return;}
    liveSession=status.session;showVersion(status.version);
    connection.innerHTML=state.source==='fixture'?'<span></span>本地测试数据':'<span></span>已连接主机';connection.classList.remove('off');
  }catch{
    liveDisconnected=true;showVersion(null);connection.innerHTML='<span></span>插件已断开，等待重连';connection.classList.add('off');
  }
  setTimeout(watchConnection,5000);
}
function makeDemo(){
  const names=['仁王 3','鬼武者 Way of Sword','仁王 2'];
  const games=names.map((name,index)=>{const trophies=Array.from({length:index===0?51:index===1?32:44},(_,id)=>({id,name:id===0?'传奇的起点':id===1?'踏上征途':id===2?'百战磨炼':id===3?'探索的足迹':`试炼 ${String(id).padStart(2,'0')}`,detail:id===0?'获得所有奖杯。':id===1?'完成旅程中的第一场挑战。':'在冒险中完成对应的挑战目标。',grade:id===0?'platinum':id<4?'gold':id<11?'silver':'bronze',hidden:id%9===0,groupId:id>44?'001':'000',unlocked:id>index*8+1&&id<38,date:id>index*8+1&&id<38?new Date(Date.UTC(2026,9,20+Math.floor(id/10),8,id)).toISOString():null,icon:null}));const groups=[...new Set(trophies.map(t=>t.groupId))].map(id=>{const ts=trophies.filter(t=>t.groupId===id);return {id,name:id==='000'?'正式版游戏':'追加内容',trophies:ts,...summary(ts)};});return {id:`DEMO${index}`,name,platform:'PS5',icon:null,trophies,groups,...summary(trophies)};});return {users:[{id:'demo',name:'演示用户',sets:[]}],games};
}
document.addEventListener('click',event=>{const home=event.target.closest('a[href="#/"]');if(!home||event.ctrlKey||event.metaKey||event.shiftKey||event.altKey)return;event.preventDefault();state.query='';state.status='all';state.grade='all';state.trophyQuery='';if(location.hash==='#/')render();else location.hash='/';window.scrollTo(0,0);});
window.addEventListener('hashchange',()=>{state.status='all';state.grade='all';state.trophyQuery='';render();window.scrollTo(0,0);});
refresh().finally(watchConnection);
