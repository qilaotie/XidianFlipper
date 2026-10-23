const HEX32 = /^[a-f0-9]{32}$/;
const TOKEN = /^Bearer ([a-f0-9]{64})$/;
const integer = (x, lo, hi) => Number.isInteger(x) && x >= lo && x <= hi;
export function validate(data) {
  if (!data || !HEX32.test(data.id) || typeof data.nickname !== 'string' ||
      !/^[A-Za-z0-9 _.-]{1,16}$/.test(data.nickname)) return null;
  const p = data.pet;
  if (!p || !integer(p.age,0,4294967295) || !integer(p.form,0,3) ||
      !integer(p.coins,0,9999) || !integer(p.dex,0,255) || !integer(p.decor,0,7) ||
      !['fullness','mood','energy','clean'].every(k=>integer(p[k],0,100)) ||
      typeof p.sleeping !== 'boolean') return null;
  if (!Array.isArray(data.scores) || data.scores.length > 2) return null;
  const seen = new Set();
  for (const s of data.scores) {
    if (!s || !['snake','tetris'].includes(s.game) || seen.has(s.game) ||
        !integer(s.score,0,s.game==='snake'?189:100000000) ||
        !integer(s.progress,0,s.game==='snake'?192:1000000)) return null;
    if (s.game==='snake' && s.progress!==s.score+3) return null;
    seen.add(s.game);
  }
  // Whitelist fields: never store Wi-Fi settings, addresses, or arbitrary client fields.
  return {id:data.id,nickname:data.nickname.trim()||'Player',pet:{
    age:p.age,form:p.form,coins:p.coins,dex:p.dex,decor:p.decor,
    fullness:p.fullness,mood:p.mood,energy:p.energy,clean:p.clean,sleeping:p.sleeping
  },scores:data.scores.map(s=>({game:s.game,score:s.score,progress:s.progress}))};
}
async function digest(value) {
  return Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',new TextEncoder().encode(value))),b=>b.toString(16).padStart(2,'0')).join('');
}
export default {
  async fetch(request, env, ctx) {
    const url = new URL(request.url), origin = request.headers.get('Origin');
    const headers = {
      'Content-Type':'application/json; charset=utf-8',
      'X-Content-Type-Options':'nosniff','Cache-Control':'no-store',
      'Access-Control-Allow-Origin':env.PUBLIC_ORIGIN,
      'Access-Control-Allow-Methods':'GET,POST,DELETE,OPTIONS',
      'Access-Control-Allow-Headers':'Content-Type,Authorization',
      'Vary':'Origin'
    };
    const reply = (body,status=200,extra={}) => new Response(JSON.stringify(body),{status,headers:{...headers,...extra}});
    if (origin && origin!==env.PUBLIC_ORIGIN && !['http://127.0.0.1:8080','http://localhost:8080'].includes(origin)) return reply({error:'origin_not_allowed'},403);
    if (origin?.startsWith('http://')) headers['Access-Control-Allow-Origin']=origin;
    if (request.method==='OPTIONS') return reply({ok:true});
    if (url.pathname==='/health' && request.method==='GET') return reply({ok:true,service:'XidianFlipper community',version:1});
    const ip = request.headers.get('CF-Connecting-IP')||'local';
    try {
      if (request.method==='GET') {
        if (!(await env.READ_LIMIT.limit({key:ip})).success) return reply({error:'rate_limited'},429);
        let game=url.searchParams.get('game')||'snake';
        if (!['/v1/leaderboard','/v1/pets'].includes(url.pathname)) return reply({error:'not_found'},404);
        if (url.pathname==='/v1/leaderboard' && !['snake','tetris'].includes(game)) return reply({error:'invalid_game'},400);
        // Normalize cache keys, preventing arbitrary query strings from defeating caching.
        const cacheKey=new Request(`${url.origin}${url.pathname}${url.pathname.endsWith('leaderboard')?'?game='+game:''}`);
        const cached=await caches.default.match(cacheKey);
        if (cached) return new Response(cached.body,{headers:{...headers,'Cache-Control':'public, max-age=30'}});
        let body;
        if (url.pathname==='/v1/leaderboard') {
          const rows=await env.DB.prepare(`SELECT d.id,d.nickname,s.score,s.progress,s.achieved_at AS updated_at FROM scores s JOIN devices d ON d.id=s.device_id WHERE s.game=? AND d.visible=1 ORDER BY s.score DESC,s.achieved_at ASC,s.device_id ASC LIMIT 100`).bind(game).all();
          body={game,items:rows.results};
        } else {
          const rows=await env.DB.prepare('SELECT id,nickname,pet,updated_at FROM devices WHERE visible=1 ORDER BY updated_at DESC,id LIMIT 100').all();
          body={items:rows.results.map(r=>({...r,pet:JSON.parse(r.pet)}))};
        }
        const response=reply({...body,generated_at:Math.floor(Date.now()/1000)},200,{'Cache-Control':'public, max-age=30'});
        ctx.waitUntil(caches.default.put(cacheKey,response.clone()));
        return response;
      }
      if (!['POST','DELETE'].includes(request.method) || url.pathname!=='/v1/sync') return reply({error:'not_found'},404);
      const token=TOKEN.exec(request.headers.get('Authorization')||'');
      if (!token) return reply({error:'unauthorized'},401);
      if (!(await env.WRITE_LIMIT.limit({key:ip})).success) return reply({error:'rate_limited'},429,{'Retry-After':'60'});
      if (!request.headers.get('Content-Type')?.startsWith('application/json')) return reply({error:'json_required'},415);
      // Bound even chunked bodies; do not allocate an unbounded request.text().
      const reader=request.body?.getReader();
      if (!reader) return reply({error:'invalid_json'},400);
      let size=0; const chunks=[];
      while(true) { const {done,value}=await reader.read(); if(done)break; size+=value.length;
        if(size>2048){await reader.cancel();return reply({error:'body_too_large'},413);} chunks.push(value); }
      const bytes=new Uint8Array(size);let pos=0;for(const c of chunks){bytes.set(c,pos);pos+=c.length;}
      let raw; try { raw=JSON.parse(new TextDecoder().decode(bytes)); } catch {return reply({error:'invalid_json'},400);}
      const data=request.method==='DELETE'?(HEX32.test(raw?.id)?{id:raw.id}:null):validate(raw);
      if (!data) return reply({error:'invalid_payload'},400);
      const hash=await digest(token[1]);
      const device=await env.DB.prepare('SELECT token_hash,updated_at FROM devices WHERE id=?').bind(data.id).first();
      if (device && device.token_hash!==hash) return reply({error:'unauthorized'},401);
      const now=Math.floor(Date.now()/1000);
      if (request.method==='DELETE') {
        // Idempotent removal also covers a device that never completed its first upload.
        if (!device) return reply({ok:true,hidden:true});
        await env.DB.batch([
          env.DB.prepare("UPDATE devices SET visible=0,nickname='Hidden',pet='{}' WHERE id=? AND token_hash=?").bind(data.id,hash),
          env.DB.prepare('DELETE FROM scores WHERE device_id=?').bind(data.id)
        ]);
        return reply({ok:true,hidden:true});
      }
      if (device && now-device.updated_at<60) return reply({error:'sync_too_soon'},429,{'Retry-After':'60'});
      // Atomically enforce enrollment capacity; a random 128-bit identity is not a MAC address.
      if (!device) await env.DB.prepare(`INSERT INTO devices(id,token_hash,nickname,pet,updated_at) SELECT ?,?,?,?,? WHERE (SELECT COUNT(*) FROM devices)<? ON CONFLICT(id) DO NOTHING`).bind(data.id,hash,data.nickname,JSON.stringify(data.pet),now,Number(env.MAX_DEVICES)||2000).run();
      const owner=await env.DB.prepare('SELECT token_hash FROM devices WHERE id=?').bind(data.id).first();
      if (!owner) return reply({error:'community_full'},503);
      if (owner.token_hash!==hash) return reply({error:'unauthorized'},401);
      const statements=[env.DB.prepare('UPDATE devices SET nickname=?,pet=?,updated_at=?,visible=1 WHERE id=? AND token_hash=? AND updated_at<=?').bind(data.nickname,JSON.stringify(data.pet),now,data.id,hash,now)];
      for(const s of data.scores) statements.push(env.DB.prepare(`INSERT INTO scores(device_id,game,score,progress,achieved_at) VALUES(?,?,?,?,?) ON CONFLICT(device_id,game) DO UPDATE SET score=excluded.score,progress=excluded.progress,achieved_at=excluded.achieved_at WHERE excluded.score>scores.score`).bind(data.id,s.game,s.score,s.progress,now));
      await env.DB.batch(statements);
      return reply({ok:true,synced_at:now,next_sync_after:60});
    } catch {
      // Never echo SQL errors, request bodies, credentials, or account details to clients/logs.
      return reply({error:'temporarily_unavailable'},503,{'Retry-After':'60'});
    }
  }
};
