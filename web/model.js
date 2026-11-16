export const grades = ['platinum', 'gold', 'silver', 'bronze'];
const gradeMap = {p:'platinum',g:'gold',s:'silver',b:'bronze',platinum:'platinum',gold:'gold',silver:'silver',bronze:'bronze'};
const weights = {platinum:300,gold:90,silver:30,bronze:15};
export function timestamp(us) {
  try {const value=BigInt(us); if(value<=0n)return null; const ms=(value-62135596800000000n)/1000n; const n=Number(ms); if(n<0||n>Date.now()+86400000)return null;return new Date(n).toISOString();}catch{return null;}
}
export function summary(trophies) {
  const counts=Object.fromEntries(grades.map(g=>[g,0]));let earned=0,unknown=0,points=0,totalPoints=0;
  for(const t of trophies){const w=weights[t.grade]??0;totalPoints+=w;if(t.unlocked===true){earned++;points+=w;if(t.grade in counts)counts[t.grade]++;}else if(t.unlocked!==false)unknown++;}
  return {total:trophies.length,earned,unknown,counts,progress:unknown||trophies.some(t=>!grades.includes(t.grade))?null:(totalPoints?Math.floor(points/totalPoints*100):null)};
}
export function normalize(id,conf,meta,state,base) {
  if(!Array.isArray(conf?.trophies)||!conf.trophies.length)throw Error('奖杯定义缺失');
  const md=meta?.metadata??{};
  const names=new Map((md.trophyMetadata??[]).map(t=>[Number(t.id),t]));
  const rows=new Map((state?.rows??[]).map(t=>[Number(t.id),t]));
  const groupNames=new Map((md.groupMetadata??[]).map(g=>[String(g.id),g.name]));
  const seen=new Set();
  const trophies=conf.trophies.map(t=>{
    const num=Number(t.id);if(!Number.isInteger(num)||num<0||num>4095||seen.has(num))throw Error('奖杯编号不合法或重复');seen.add(num);
    const name=names.get(num)??{},row=rows.get(num),grade=gradeMap[String(t.grade).toLowerCase()]??'unknown';
    return {id:num,name:name.name||`奖杯 ${num}`,detail:name.detail||'',grade,hidden:t.hidden===true,groupId:String(t.groupId??'000'),unlocked:row?.unlocked===true?true:row?.unlocked===false?false:null,date:row?.unlocked?timestamp(row.timestampUs):null,icon:`${base}/trop${String(num).padStart(4,'0')}.png`};
  });
  const groupIds=[...new Set(trophies.map(t=>t.groupId))];
  const groups=groupIds.map((gid,index)=>{const ts=trophies.filter(t=>t.groupId===gid);return {id:gid,name:groupNames.get(gid)||(index===0?'正式版游戏':`追加内容 ${index}`),trophies:ts,...summary(ts)};});
  return {id,name:md.titleMetadata?.name||id,platform:'PS5',icon:`${base}/icon`,trophies,groups,...summary(trophies)};
}
export function filterTrophies(trophies,{status='all',grade='all',sort='id',query=''}={}) {
  const q=query.trim().toLocaleLowerCase();return trophies.filter(t=>(status==='all'||(status==='earned'?t.unlocked===true:status==='locked'?t.unlocked===false:t.unlocked===null))&&(grade==='all'||t.grade===grade)&&(!q||`${t.name} ${t.detail}`.toLocaleLowerCase().includes(q))).sort((a,b)=>sort==='recent'?(b.date??'').localeCompare(a.date??'')||a.id-b.id:sort==='grade'?grades.indexOf(a.grade)-grades.indexOf(b.grade)||a.id-b.id:a.id-b.id);
}
